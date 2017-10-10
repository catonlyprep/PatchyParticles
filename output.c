/*
 * output.c
 *
 *  Created on: 31/ott/2011
 *      Author: lorenzo
 */

#include "output.h"
#include "utils.h"

#include <unistd.h>

void output_init(input_file *input, Output *output_files) {
	const char mode[2] = "w";
	char name[512];
	if(getInputString(input, "Log_file", name, 0) == KEY_FOUND) {
		if(strcmp("none", name) != 0) {
			FILE *mylog = fopen(name, "w");
			if(mylog == NULL) output_exit(output_files, "Log file '%s' is not writable\n", name);
			output_files->log = mylog;
		}
	}

	sprintf(name, "energy.dat");
	getInputString(input, "Energy_file", name, 0);
	output_files->energy = fopen(name, mode);
	if(output_files->energy == NULL) output_exit(output_files, "Energy file '%s' is not writable\n", name);

	sprintf(name, "acceptance.dat");
	getInputString(input, "Acceptance_file", name, 0);
	output_files->acc = fopen(name, mode);
	if(output_files->acc == NULL) output_exit(output_files, "Acceptance file '%s' is not writable\n", name);

	getInputString(input, "Configuration_folder", output_files->configuration_folder, 1);
	if(access(output_files->configuration_folder, W_OK) != 0) {
		output_exit(output_files, "Cannot create files in directory '%s': please make sure that the directory exists and it is writable\n", output_files->configuration_folder);
	}
	sprintf(output_files->configuration_last, "last.rrr");
	getInputString(input, "Configuration_last", output_files->configuration_last, 0);

	int ensemble;
	getInputInt(input, "Ensemble", &ensemble, 1);
	if(ensemble != 0) {
		sprintf(name, "density.dat");
		getInputString(input, "Density_file", name, 0);
		output_files->density = fopen(name, mode);
		if(output_files->density == NULL) output_exit(output_files, "Density file '%s' is not writable\n", name);

		if(ensemble == 3 || ensemble == 6) {
			getInputString(input, "Umbrella_sampling_folder", output_files->sus_folder, 1);
			if(access(output_files->sus_folder, W_OK) != 0) {
				output_exit(output_files, "Cannot create files in directory '%s': please make sure that the directory exists and it is writable\n", output_files->sus_folder);
			}
		}
	}
	else output_files->density = NULL;
}

void output_free(Output *IO) {
	fclose(IO->energy);
	fclose(IO->acc);
	if(IO->log != stderr) fclose(IO->log);
	if(IO->density != NULL) fclose(IO->density);
}

void output_print(Output *IO, System *syst, llint step) {
	double E = (syst->N > 0) ? syst->energy / syst->N : 0;

	fprintf(IO->energy, "%lld %lf\n", step, E);
	fflush(IO->energy);

	if(syst->ensemble != 0) {
		fprintf(IO->density, "%lld %lf %d\n", step, syst->N / syst->V, syst->N);
		fflush(IO->density);
	}

	// acceptances
	fprintf(IO->acc, "%lld %e", step, syst->accepted[ROTO_TRASL] / (double) syst->tries[ROTO_TRASL]);
	// TODO: update when more moves will become available
	if(syst->dynamics != 0) {
		if (syst->dynamics == 1) // VMMC
			fprintf(IO->acc, " %e", syst->accepted[MOVE_VMMC] / (double) syst->tries[MOVE_VMMC]);
		else
			fprintf(IO->acc, " %e", syst->accepted[AVB] / (double) syst->tries[AVB]);
	}
	if(syst->ensemble != 0 && syst->dynamics != 1) {
		fprintf(IO->acc, " %e", syst->accepted[ADD]/ (double) syst->tries[ADD]);
		fprintf(IO->acc, " %e", syst->accepted[REMOVE]/ (double) syst->tries[REMOVE]);
	}

	fprintf(IO->acc, "\n");
	fflush(IO->acc);
	utils_reset_acceptance_counters(syst);

	printf("%lld %lf %lf", step, E, syst->energy);
	if(syst->ensemble != 0) printf(" %lf", syst->N / syst->V);
	printf("\n");

	output_save(IO, syst, step, IO->configuration_last);
}

void output_save(Output *IO, System *syst, llint step, char *name) {
	FILE *out = fopen(name, "w");
	if(out == NULL) output_exit(IO, "File '%s' is not writable\n", name);

	fprintf(out, "%lld %d %lf %lf %lf\n", step, syst->N, syst->L, syst->L, syst->L);

	int i;
	PatchyParticle *p = syst->particles;
	for(i = 0; i < syst->N; i++) {
		fprintf(out, "%lf %lf %lf\n", p->orientation[0][0], p->orientation[0][1], p->orientation[0][2]);
		fprintf(out, "%lf %lf %lf\n", p->orientation[1][0], p->orientation[1][1], p->orientation[1][2]);
		fprintf(out, "%.12lf %.12lf %.12lf\n", p->r[0], p->r[1], p->r[2]);
		p++;
	}
	fclose(out);
}

void output_sus(Output *IO, System *syst, llint step) {
	char name[512];
	sprintf(name, "%s/sus-%lld.dat", IO->sus_folder, step);
	FILE *out = fopen(name, "w");
	if(out == NULL) output_exit(IO, "SUS file '%s' is not writable\n", name);

	int i;
	for(i = 0; i < (syst->N_max - syst->N_min + 1); i++) {
		if(syst->SUS_hist[i] > 0) fprintf(out, "%d %lld\n", i + syst->N_min, syst->SUS_hist[i]);
		syst->SUS_hist[i] = 0;
	}

	fclose(out);
}

void output_e_sus(Output *IO, System *syst, llint step) {
	char name[512];
	sprintf(name, "%s/sus-%lld.dat", IO->sus_folder, step);
	FILE *out = fopen(name, "w");
	if(out == NULL) output_exit(IO, "SUS file '%s' is not writable\n", name);

	int i, j;
	for(i = 0; i < (syst->N_max - syst->N_min + 1); i++) {
		for(j = 0; j < syst->SUS_e_bins; j++) {
			llint c = syst->SUS_e_hist[i][j];
			if(c > 0) fprintf(out, "%d %d %lld\n", i + syst->N_min, j, c);
			syst->SUS_e_hist[i][j] = 0;
		}
	}

	fclose(out);
}

void output_log_msg(Output *IO, char *format, ...) {
	va_list args;
	if(IO->log != stderr) {
		va_start(args, format);
		vfprintf(IO->log, format, args);
		va_end(args);
	}

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fflush(IO->log);
}

void output_exit(Output *IO, char *format, ...) {
	va_list args;
	if(IO->log != stderr) {
		va_start(args, format);
		vfprintf(IO->log, format, args);
		va_end(args);
	}

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	fflush(IO->log);
	exit(1);
}

void output_exit_stderr(char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	exit(1);
}