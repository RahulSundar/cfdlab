#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#define NP		25000
#define NTRI	2*NP
#define max(a,b)  (a>b?a:b)
#define min(a,b)  (a<b?a:b)

//Flag for point types
#define INTERIOR        0
#define SOLID_WALL      3
#define OUTER_BOUND     5

double x[NP], y[NP], angle[NP], cond[NP];
int np, ntri, Nbr, nsolidPoints[20], nouterPoints;
int tri[NTRI][3];
int type[NP], solidEdge[1000][2], outerEdge[1000][2];
char *outfile;

/* ----------------------------------------------------------------------- */
int main(int argc, char *argv[])
/* ----------------------------------------------------------------------- */
{
   int i, j, m, dummy;
   double dd;
   char *dplfile;
   char line[80];
   size_t len = 0;
   ssize_t read;
   FILE *fpt;
   void SetType();
   void PrintAmdba();
   void vigie();

   if(argc < 3) {
      printf
         ("Converts a .dpl grid file generated by delaundo into amdba format\n");
      printf("Usage: %s <dpl file> <output file>\n", argv[0]);
      exit(0);
   }
   dplfile = argv[1];
   outfile = argv[2];
   fpt = fopen(dplfile, "r");
   if(fpt == NULL) {
      printf("Could not open file %s\n", dplfile);
      exit(0);
   }
   //getline(&line, &len, fpt);   /* First line has some text */
   fgets(line, 80, fpt);   /* First line has some text */
   fscanf(fpt, "%d%d%d", &ntri, &dummy, &dummy);
   if(ntri > NTRI) {
      printf("Increase size of NTRI to atleast %d\n", ntri);
      exit(0);
   }
   printf("Number of triangles    = %d\n", ntri);
   for(i = 0; i < ntri; i++) {
      fscanf(fpt, "%d%d%d%d%d%d%d%d", &dummy, &tri[i][0], &tri[i][1],
             &tri[i][2], &dummy, &dummy, &dummy, &dummy);
   }
   fscanf(fpt, "%d", &np);
   if(np > NP) {
      printf("Increase size of NP to atleast %d\n", np);
      exit(0);
   }
   printf("Number of points       = %d\n", np);
   fscanf(fpt, "%lf%lf%lf%lf%lf%lf", &dd, &dd, &dd, &dd, &dd, &dd);
   for(i = 0; i < np; i++)
      fscanf(fpt, "%lf%lf%lf%lf%lf%lf%d", &x[i], &y[i], &dd, &dd, &dd, &dd,
             &dummy);
   fscanf(fpt, "%d%d", &Nbr, &dummy);
   printf("Number of boundaries = %d\n", Nbr);
   for(m = 0; m < Nbr; m++) {
      fscanf(fpt, "%d", &nsolidPoints[m]);
      printf("%5d ", nsolidPoints[m]);
   }
   printf("\n");
   fclose(fpt);

   SetType();
   PrintAmdba();
   vigie();
}

/* ----------------------------------------------------------------------- */
void SetType()
/* ----------------------------------------------------------------------- */
{
   int i, status;

   status = 0;
   for(i = 0; i < Nbr; i++)
      if(nsolidPoints[i] < 0)
         status += 1;

   if(status > 0) {
      printf("* There seems to be a dummy boundary.\n");
      printf("* Make sure that any dummy boundaries are\n");
      printf("* at the end of the list.\n");
      printf("Reducing number of boundary by %d\n", status);
      Nbr = Nbr - status;
   }

   for(i = 0; i < np; i++)
      type[i] = -1;

   for(i = 0; i < nsolidPoints[Nbr - 2]; i++)
      type[i] = SOLID_WALL;

   for(i = nsolidPoints[Nbr - 2]; i < nsolidPoints[Nbr - 1]; i++)
      type[i] = OUTER_BOUND;

   for(i = nsolidPoints[Nbr - 1]; i < np; i++) {
      type[i] = INTERIOR;
   }

   for(i = 0; i < np; i++)
      if(type[i] == -1) {
         printf("SetType: Point type not assigned for node %d\n", i);
         exit(0);
      }
}

/* ----------------------------------------------------------------------- */
void PrintAmdba()
/* ----------------------------------------------------------------------- */
{
   int i, j;
   FILE *fpt;

   fpt = fopen(outfile, "w");
   if(fpt == NULL) {
      printf("Could not open file %s\n", outfile);
      exit(0);
   }
   fprintf(fpt, "%d    %d\n", np, ntri);
   for(i = 0; i < np; i++)
      fprintf(fpt, "%7d %15.7e %15.7e %5d\n", i + 1, x[i], y[i], type[i]);
   for(i = 0; i < ntri; i++)
      fprintf(fpt, "%5d %8d %8d %8d %5d\n", i + 1, tri[i][0], tri[i][1],
              tri[i][2], 0);
   printf("Grid data written into %s in amdba format\n", outfile);
}

/* ----------------------------------------------------------------------- */
void vigie()
/* ----------------------------------------------------------------------- */
{
   int i;
   FILE *fpt;

   fpt = fopen("vg.dat", "w");
   fprintf(fpt, "points    %d\n", np);
   for(i = 0; i < np; i++)
      fprintf(fpt, "%15.6lf %15.6lf\n", x[i], y[i]);
   fprintf(fpt, "triangles    %d\n", ntri);
   for(i = 0; i < ntri; i++)
      fprintf(fpt, "%8d %8d %8d\n", tri[i][0], tri[i][1], tri[i][2]);
   fclose(fpt);
   printf("Grid data written into vg.dat in vigie format\n");
}
