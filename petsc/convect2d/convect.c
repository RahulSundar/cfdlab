static char help[] = "Solves u_t + u_x = 0.\n\n";

#include <petscsys.h>
#include <petscdm.h>
#include <petscdmda.h>
#include <petscvec.h>
#include <petscviewerhdf5.h>

const double ark[] = {0.0, 3.0/4.0, 1.0/3.0};
const double xmin = 0.0, xmax = 1.0;
const double ymin = 0.0, ymax = 1.0;

double initcond(double x, double y)
{
   return sin(2*M_PI*x) * sin(2*M_PI*y);
}

//------------------------------------------------------------------------------
// Weno reconstruction
// Return left value for face between u0, up1
//------------------------------------------------------------------------------
double weno5(double um2, double um1, double u0, double up1, double up2)
{
   double eps = 1.0e-6;
   double gamma1=1.0/10.0, gamma2=3.0/5.0, gamma3=3.0/10.0;
   double beta1, beta2, beta3;
   double u1, u2, u3;
   double w1, w2, w3;

   beta1 = (13.0/12.0)*pow((um2 - 2.0*um1 + u0),2) +
           (1.0/4.0)*pow((um2 - 4.0*um1 + 3.0*u0),2);
   beta2 = (13.0/12.0)*pow((um1 - 2.0*u0 + up1),2) +
           (1.0/4.0)*pow((um1 - up1),2);
   beta3 = (13.0/12.0)*pow((u0 - 2.0*up1 + up2),2) +
           (1.0/4.0)*pow((3.0*u0 - 4.0*up1 + up2),2);

   w1 = gamma1 / pow(eps+beta1, 2);
   w2 = gamma2 / pow(eps+beta2, 2);
   w3 = gamma3 / pow(eps+beta3, 2);

   u1 = (1.0/3.0)*um2 - (7.0/6.0)*um1 + (11.0/6.0)*u0;
   u2 = -(1.0/6.0)*um1 + (5.0/6.0)*u0 + (1.0/3.0)*up1;
   u3 = (1.0/3.0)*u0 + (5.0/6.0)*up1 - (1.0/6.0)*up2;

   return (w1 * u1 + w2 * u2 + w3 * u3)/(w1 + w2 + w3);
}

//------------------------------------------------------------------------------
// Saves solution into files sol000.h5, sol001.h5, etc.
// You can open them in VisIt.
//------------------------------------------------------------------------------
PetscErrorCode savesol(int *c, Vec ug)
{
   PetscErrorCode ierr;
   char           filename[32] = "sol";
   PetscViewer    viewer;
   sprintf(filename, "sol%03d.h5", *c);
   ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD, filename, FILE_MODE_WRITE, &viewer);  CHKERRQ(ierr);
   ierr = VecView(ug, viewer); CHKERRQ(ierr);
   ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
   ++(*c);
   return(0);
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
   // some parameters that can overwritten from command line
   PetscReal Tf  = 10.0;
   PetscReal cfl = 0.4;
   PetscInt  si  = 100;
   PetscInt  nx  = 50, ny=50; // use -da_grid_x, -da_grid_y to override these
   
   PetscErrorCode ierr;
   DM       da;
   Vec      ug, ul;
   PetscInt i, j, ibeg, jbeg, nlocx, nlocy;
   const PetscInt sw = 3, ndof = 1; // stencil width
   PetscMPIInt rank, size;
   PetscScalar **u;
   PetscScalar **unew;
   int c = 0; // counter for saving solution files

   ierr = PetscInitialize(&argc, &argv, (char*)0, help); CHKERRQ(ierr);

   MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
   MPI_Comm_size(PETSC_COMM_WORLD, &size);
   
   // Get some command line options
   ierr = PetscOptionsGetReal(NULL,NULL,"-Tf",&Tf,NULL); CHKERRQ(ierr);
   ierr = PetscOptionsGetReal(NULL,NULL,"-cfl",&cfl,NULL); CHKERRQ(ierr);
   ierr = PetscOptionsGetInt(NULL,NULL,"-si",&si,NULL); CHKERRQ(ierr);

   ierr = DMDACreate2d(PETSC_COMM_WORLD, DM_BOUNDARY_PERIODIC, DM_BOUNDARY_PERIODIC,
                       DMDA_STENCIL_STAR, -nx, -ny, PETSC_DECIDE, PETSC_DECIDE, ndof,
                       sw, NULL, NULL, &da); CHKERRQ(ierr);
   ierr = DMDAGetInfo(da,0,&nx,&ny,0,0,0,0,0,0,0,0,0,0); CHKERRQ(ierr);
   PetscReal dx = (xmax - xmin) / (PetscReal)(nx);
   PetscReal dy = (ymax - ymin) / (PetscReal)(ny);
   PetscPrintf(PETSC_COMM_WORLD,"nx = %d, dx = %e\n", nx, dx);
   PetscPrintf(PETSC_COMM_WORLD,"ny = %d, dy = %e\n", ny, dy);

   ierr = DMCreateGlobalVector(da, &ug); CHKERRQ(ierr);
   ierr = PetscObjectSetName((PetscObject) ug, "Solution"); CHKERRQ(ierr);

   ierr = DMDAGetCorners(da, &ibeg, &jbeg, 0, &nlocx, &nlocy, 0); CHKERRQ(ierr);
   ierr = DMDAVecGetArray(da, ug, &u); CHKERRQ(ierr);
   for(j=jbeg; j<jbeg+nlocy; ++j)
      for(i=ibeg; i<ibeg+nlocx; ++i)
      {
         PetscReal x = xmin + i*dx + 0.5*dx;
         PetscReal y = ymin + j*dy + 0.5*dy;
         u[j][i] = initcond(x,y);
      }
   ierr = DMDAVecRestoreArray(da, ug, &u); CHKERRQ(ierr);
   ierr = savesol(&c, ug); CHKERRQ(ierr);

   // Get local view
   ierr = DMGetLocalVector(da, &ul); CHKERRQ(ierr);

   PetscInt il, jl, nl, ml;
   ierr = DMDAGetGhostCorners(da,&il,&jl,0,&nl,&ml,0); CHKERRQ(ierr);

   double res[nlocx][nlocy], uold[nlocx][nlocy];
   double umax = sqrt(2.0);
   double dt = cfl * dx / umax;
   double lam= dt/(dx*dy);

   double t = 0.0;
   int it = 0;

   while(t < Tf)
   {
      if(t+dt > Tf) dt = Tf - t;
      for(int rk=0; rk<3; ++rk)
      {
         ierr = DMGlobalToLocalBegin(da, ug, INSERT_VALUES, ul); CHKERRQ(ierr);
         ierr = DMGlobalToLocalEnd(da, ug, INSERT_VALUES, ul); CHKERRQ(ierr);

         ierr = DMDAVecGetArrayRead(da, ul, &u); CHKERRQ(ierr);
         ierr = DMDAVecGetArray(da, ug, &unew); CHKERRQ(ierr);

         if(rk==0)
            for(i=ibeg; i<ibeg+nlocx; ++i)
               for(j=jbeg; j<jbeg+nlocy; ++j)
                  uold[i-ibeg][j-jbeg] = u[j][i];

         for(i=0; i<nlocx; ++i)
            for(j=0; j<nlocy; ++j)
               res[i][j] = 0.0;

         // x fluxes
         for(i=0; i<nlocx+1; ++i)
            for(j=0; j<nlocy; ++j)
            {
               // face between k-1, k
               int k = il+sw+i;
               int l = jl+sw+j;
               double uleft = weno5(u[l][k-3],u[l][k-2],u[l][k-1],u[l][k],u[l][k+1]);
               double flux = uleft * dy;
               if(i==0)
               {
                  res[i][j] -= flux;
               }
               else if(i==nlocx)
               {
                  res[i-1][j] += flux;
               }
               else
               {
                  res[i][j]   -= flux;
                  res[i-1][j] += flux;
               }
            }

         // y fluxes
         for(j=0; j<nlocy+1; ++j)
            for(i=0; i<nlocx; ++i)
            {
               // face between l-1, l
               int k = il+sw+i;
               int l = jl+sw+j;
               double uleft = weno5(u[l-3][k],u[l-2][k],u[l-1][k],u[l][k],u[l+1][k]);
               double flux = uleft * dx;
               if(j==0)
               {
                  res[i][j] -= flux;
               }
               else if(j==nlocy)
               {
                  res[i][j-1] += flux;
               }
               else
               {
                  res[i][j]   -= flux;
                  res[i][j-1] += flux;
               }
            }

         // Update solution
         for(i=ibeg; i<ibeg+nlocx; ++i)
            for(j=jbeg; j<jbeg+nlocy; ++j)
               unew[j][i] = ark[rk]*uold[i-ibeg][j-jbeg] + (1-ark[rk])*(u[j][i] - lam * res[i-ibeg][j-jbeg]);

         ierr = DMDAVecRestoreArrayRead(da, ul, &u); CHKERRQ(ierr);
         ierr = DMDAVecRestoreArray(da, ug, &unew); CHKERRQ(ierr);
      }

      t += dt; ++it;
      PetscPrintf(PETSC_COMM_WORLD,"it, t = %d, %f\n", it, t);
      if(it%si == 0)
      {
         ierr = savesol(&c, ug); CHKERRQ(ierr);
      }
   }

   // Destroy everything before finishing
   ierr = VecDestroy(&ug); CHKERRQ(ierr);
   ierr = DMDestroy(&da); CHKERRQ(ierr);

   ierr = PetscFinalize(); CHKERRQ(ierr);
}