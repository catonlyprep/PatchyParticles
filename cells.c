/*
 * cells.c
 *
 *  Created on: 16 Oct 2017
 *      Author: lorenzo
 */

#include "cells.h"
#include "defs.h"
#include "output.h"

#include <math.h>
#include <float.h>
#include <stdlib.h>

void cells_init(System *syst, Output *output_files) {
	syst->cells = (Cells *) malloc(sizeof(Cells));

	Cells *cells = syst->cells;
	cells->N_side = floor(syst->box[0] / (1. + syst->kf_delta));
	if(cells->N_side < 3) {
		cells->N_side = 3;
		output_log_msg(output_files, "Box side is too small, setting cells.N_side = 3\n");
	}
	cells->N = cells->N_side * cells->N_side * cells->N_side;
	cells->heads = malloc(sizeof(PatchyParticle *) * cells->N);
	output_log_msg(output_files, "Cells per side: %d, total: %d\n", cells->N_side, cells->N);

	int i, ind[3];
	for(i = 0; i < cells->N; i++) cells->heads[i] = NULL;
	for(i = 0; i < syst->N; i++) {
		PatchyParticle *p = syst->particles + i;

		ind[0] = (int) ((p->r[0] / syst->box[0] - floor(p->r[0] / syst->box[0])) * (1. - DBL_EPSILON) * cells->N_side);
		ind[1] = (int) ((p->r[1] / syst->box[1] - floor(p->r[1] / syst->box[1])) * (1. - DBL_EPSILON) * cells->N_side);
		ind[2] = (int) ((p->r[2] / syst->box[2] - floor(p->r[2] / syst->box[2])) * (1. - DBL_EPSILON) * cells->N_side);

		int cell_index = (ind[0] * cells->N_side + ind[1]) * cells->N_side + ind[2];
		p->next = cells->heads[cell_index];
		cells->heads[cell_index] = p;
		p->cell = cell_index;
		p->cell_old = cell_index;
	}
}

void cells_check(System *syst, Output *output_files) {
	int i, counter;
	for(i = 0, counter = 0; i < syst->cells->N; i++) {
		PatchyParticle *p = syst->cells->heads[i];
		while(p != NULL) {
			p = p->next;
			counter++;
		}
	}

	if(counter != syst->N) output_exit(output_files, "\nThere are %d particles in cells, should be %d\n", counter, syst->N);
}

void cells_free(Cells *cells) {
	free(cells->heads);
	free(cells);
}