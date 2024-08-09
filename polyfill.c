#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

struct edge {
    int x1, y1;
    int x2, y2;
    bool active;

    /* (x2 - x1) / (y2 - y1) as 16.16 signed fixed point */
    long inverse_slope;
};

static int compare_ints(const void *l, const void *r) {
    const int *lhs = (const int*) l;
    const int *rhs = (const int*) r;
    if(*lhs < *rhs)
	return -1;
    if(*lhs == *rhs)
	return 0;
    return 1;
}

static void find_min_max_y(struct edge *edges, int n_edges,
			   int *min_y, int *max_y) {
    int mi = edges[0].y1, ma = edges[0].y2;
    for(int i = 0; i < n_edges; i++) {
	if(edges[i].y1 < mi)
	    mi = edges[i].y1;
	if(edges[i].y2 >= ma)
	    ma = edges[i].y2;
    }
    *min_y = mi;
    *max_y = ma;
}

#define FRACBITS 16
#define ONEHALF (1 << (FRACBITS-1))
#define fp_div(x, y, z) (long)((((long long)(x)) << (z)) / ((long long)(y)))
#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))

void fill_polygon(void (*hline)(int x1, int x2, int y),
		  const int *coords, int npoints) {
    if(npoints < 3)
	return;

    /* Build edge table from coords. Horizontal edges are filtered
     * out, so n_edges <= n_points in general. */
    struct edge *edges = malloc(npoints * sizeof(struct edge));

    int n_edges = 0;

    for(int i = 0; i < npoints; i++) {
	int x1, y1, x2, y2;

	x1 = coords[(2*i+0)];
	y1 = coords[(2*i+1)];
	x2 = coords[(2*i+2) % (npoints * 2)];
	y2 = coords[(2*i+3) % (npoints * 2)];

	/* Only add if non-horizontal, and enforce y1 < y2. */
	if(y1 != y2) {
	    bool swap = y1 > y2;

	    struct edge *edge = edges + (n_edges++);

	    edge->active = false;
	    edge->x1 = swap ? x2 : x1;
	    edge->y1 = swap ? y2 : y1;
	    edge->x2 = swap ? x1 : x2;
	    edge->y2 = swap ? y1 : y2;

	    edge->inverse_slope = fp_div((edge->x2 - edge->x1) << FRACBITS,
					 (edge->y2 - edge->y1) << FRACBITS,
					 FRACBITS);
	}
    }

    int min_y, max_y;
    find_min_max_y(edges, n_edges, &min_y, &max_y);

    /* upper bound on number of intersections is n_edges */
    int *intersections = calloc(n_edges, sizeof(int));

    for(int y = min_y; y <= max_y; y++) {
	/* Update active edge set. */
	for(int i = 0; i < n_edges; i++) {
	    if(edges[i].y1 == y)
		edges[i].active = true;
	    else if(edges[i].y2 == y)
		edges[i].active = false;
	}

	/* Compute X coordinates of intersections. */
	int n_intersections = 0;
	for(int i = 0; i < n_edges; i++) {
	    if(edges[i].active) {
		int x = edges[i].x1;
		x += (edges[i].inverse_slope * (y - edges[i].y1) + ONEHALF) >> FRACBITS;
		intersections[n_intersections++] = x;
	    }
	}
	qsort(intersections, n_intersections, sizeof(int),
	      &compare_ints);

	assert(n_intersections % 2 == 0);

	/* Draw horizontal lines between successive pairs of
	 * intersections of the scanline with active edges. */
	for(int i = 0; i < n_intersections; i += 2)
	    hline(intersections[i], intersections[i+1], y);
    }

    free(intersections);
    free(edges);
}

#define STANDALONE
#ifdef STANDALONE

#include <SDL2/SDL.h>

SDL_Renderer* renderer = NULL;

void hline(int x1, int x2, int y) {
    printf("hline %d %d, %d\n", x1, x2, y);
    // Draw a line from (100, 100) to (700, 500)
    SDL_RenderDrawLine(renderer, x1, y, x2, y);
}

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

int main() {
    SDL_Window* window = NULL;
    SDL_Event event;
    int running = 1;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    window = SDL_CreateWindow("SDL2 Line Drawing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

#define DYNAMIC
#ifdef DYNAMIC
    int *poly = NULL;
    int n_points = 0;
#else
    int poly[] = {
	100, 100,
	200, 100,
	200, 200,
	100, 200
    };
    int n_points = ARRAYLEN(poly)/2;
#endif

    // Main loop
    while (running) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
#ifdef DYNAMIC
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

		    poly = realloc(poly, ++n_points * 2 * sizeof(int));
		    poly[2 * (n_points - 1)] = mouseX;
		    poly[2 * (n_points - 1) + 1] = mouseY;
                }
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_c) {
		    free(poly);
		    poly = NULL;
		    n_points = 0;
                }
            }
#endif
        }

        // Clear the screen with a black color
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Set draw color to white
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	fill_polygon(hline, poly, n_points);

        // Present the back buffer
        SDL_RenderPresent(renderer);

        // Delay to make the loop run at a reasonable speed
        SDL_Delay(16); // approximately 60 FPS
    }

    // Clean up and quit SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
#endif
