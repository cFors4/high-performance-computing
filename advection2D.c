/*******************************************************************************
2D advection example program which advects a Gaussian u(x,y) at a fixed velocity


Outputs: initial.dat - inital values of u(x,y) 
         final.dat   - final values of u(x,y)

         The output files have three columns: x, y, u

         Compile with: gcc -o advection2D -std=c99 advection2D.c -lm

Notes: The time step is calculated using the CFL condition

********************************************************************************/

/*********************************************************************
                     Include header files 
**********************************************************************/

#include <stdio.h>
#include <math.h>
#include <omp.h>

/*********************************************************************
                      Main function
**********************************************************************/

int main(){

  /* Grid properties */
  const int NX=1000;    // Number of x points
  const int NY=1000;    // Number of y points
  const float xmin=0.0; // Minimum x value
  const float xmax=30.0; // Maximum x value
  const float ymin=0.0; // Minimum y value
  const float ymax=30.0; // Maximum y value
  
  /* Parameters for the Gaussian initial conditions */
  const float x0=3.0;                    // Centre(x)
  const float y0=15.0;                    // Centre(y)
  const float sigmax=1.0;               // Width(x)
  const float sigmay=5.0;               // Width(y)
  const float sigmax2 = sigmax * sigmax; // Width(x) squared
  const float sigmay2 = sigmay * sigmay; // Width(y) squared

  /* Boundary conditions */
  const float bval_left=0.0;    // Left boudnary value
  const float bval_right=0.0;   // Right boundary value
  const float bval_lower=0.0;   // Lower boundary
  const float bval_upper=0.0;   // Upper bounary
  
  /* Time stepping parameters */
  const float CFL=0.9;   // CFL number 
  const int nsteps=800; // Number of time steps

  /* Velocity */
  const float velx=1.0; // Velocity in x direction
  const float vely=0.0; // Velocity in y direction
  
  /* Arrays to store variables. These have NX+2 elements
     to allow boundary values to be stored at both ends */
  float x[NX+2];          // x-axis values
  float y[NX+2];          // y-axis values
  float u[NX+2][NY+2];    // Array of u values
  float dudt[NX+2][NY+2]; // Rate of change of u

  float x2;   // x squared (used to calculate iniital conditions)
  float y2;   // y squared (used to calculate iniital conditions)
  
  /* Calculate distance between points */
  float dx = (xmax-xmin) / ( (float) NX);
  float dy = (ymax-ymin) / ( (float) NY);
  
  /* Calculate time step using the CFL condition */
  /* The fabs function gives the absolute value in case the velocity is -ve */
  /* float dt = CFL / ( (fabs(velx) / dx) + (fabs(vely) / dy) ); */
  float dt = (CFL / ((fabs((0.2/0.41)*log(ymax/1.0)) / dx) + (fabs(vely) / dy)));
  
  /*** Report information about the calculation ***/
  printf("Grid spacing dx     = %g\n", dx);
  printf("Grid spacing dy     = %g\n", dy);
  printf("CFL number          = %g\n", CFL);
  printf("Time step           = %g\n", dt);
  printf("No. of time steps   = %d\n", nsteps);
  printf("End time            = %g\n", dt*(float) nsteps);
  printf("Distance advected x = %g\n", velx*dt*(float) nsteps);
  printf("Distance advected y = %g\n", vely*dt*(float) nsteps);

  /*** Place x points in the middle of the cell ***/
  /* LOOP 1 */
  int i;
  #pragma omp parallel for default(none) private(i) shared(NX,dx,x)
  for (int i=0; i<NX+2; i++){
    x[i] = ( (float) i - 0.5) * dx;
  }

  /*** Place y points in the middle of the cell ***/
  /* LOOP 2 */
  int j;
  #pragma omp parallel for default(none) private(j) shared(NY,dy,y)
  for (int j=0; j<NY+2; j++){
    y[j] = ( (float) j - 0.5) * dy;
  }

  /*** Set up Gaussian initial conditions ***/
  /* LOOP 3 */
  #pragma omp parallel for default(none) private(i,j) shared(u,x,NX,x0,y,NY,y0,sigmax2,sigmay2) collapse(2) lastprivate(x2,y2)
  for (int i=0; i<NX+2; i++){
    for (int j=0; j<NY+2; j++){
      x2      = (x[i]-x0) * (x[i]-x0);
      y2      = (y[j]-y0) * (y[j]-y0);
      u[i][j] = exp( -1.0 * ( (x2/(2.0*sigmax2)) + (y2/(2.0*sigmay2)) ) );
    }
  }

  /*** Write array of initial u values out to file ***/
  /* Writing to a file that needs to be plotted, needs to be written in chronological order which parrellelism disrupts */
  FILE *initialfile;
  initialfile = fopen("initial.dat", "w");
  /* LOOP 4 */
  for (int i=0; i<NX+2; i++){
    for (int j=0; j<NY+2; j++){
      fprintf(initialfile, "%g %g %g\n", x[i], y[j], u[i][j]);
    }
  }
  fclose(initialfile);
  
  /*** Update solution by looping over time steps ***/
  /* LOOP 5 */
  /* Time steps need to happen in a linear order which making them parallel would and does disrupt */
  for (int m=0; m<nsteps; m++){
    /*** Apply boundary conditions at u[0][:] and u[NX+1][:] ***/
    /* LOOP 6 */
    #pragma omp parallel for default(none) private(j) shared(NY,bval_left,bval_right,u,NX)
    for (int j=0; j<NY+2; j++){
      u[0][j]    = bval_left;
      u[NX+1][j] = bval_right;
    }

    /*** Apply boundary conditions at u[:][0] and u[:][NY+1] ***/
    /* LOOP 7 */
    #pragma omp parallel for default(none) private(i) shared(NY,bval_lower,bval_upper,u,NX)
    for (int i=0; i<NX+2; i++){
      u[i][0]    = bval_lower;
      u[i][NY+1] = bval_upper;
    }
    
    /*** Calculate rate of change of u using leftward difference ***/
    /* Loop over points in the domain but not boundary values */
    /* LOOP 8 */
    float Vx;
    #pragma omp parallel for default(none) private(i,j,Vx) shared(NY,dudt,u,NX,velx,vely,dx,dy) collapse(2)
    for (int i=1; i<NX+1; i++){
      for (int j=1; j<NY+1; j++){
        float y;
        y = j*dy;
        if (y>1) {
          Vx = (0.2/0.41)*logf(y/1.0);
          /*printf("Vx= %g y= %d\n",Vx,y);*/
          dudt[i][j] = -1*(Vx * (u[i][j] - u[i-1][j]) /dx + vely * (u[i][j] - u[i][j-1]) / dy);
        }else{
          dudt[i][j] = -1* (0 * (u[i][j] - u[i-1][j]) / dx + vely * (u[i][j] - u[i][j-1]) / dy);
        }
      }
    }
    
    /*** Update u from t to t+dt ***/
    /* Loop over points in the domain but not boundary values */
    /* LOOP 9 */
    #pragma omp parallel for default(none) private(i,j) shared(NY,dudt,u,NX,dt) collapse(2)
    for	(int i=1; i<NX+1; i++){
      for (int j=1; j<NY+1; j++){
        u[i][j] = u[i][j] + dudt[i][j] * dt;
      }
    }
  } // time loop
  
  /*** Write array of final u values out to file ***/
  FILE *finalfile;
  finalfile = fopen("final.dat", "w");
  /* LOOP 10 */
   /* Writing to a file that needs to be plotted, needs to be written in chronological order which parrellelism disrupts */
  for (int i=0; i<NX+2; i++){
    for (int j=0; j<NY+2; j++){
      // printf("y= %f\n",u[i][j]);
      fprintf(finalfile, "%g %g %g\n", x[i], y[j], u[i][j]);
    }
  }
  fclose(finalfile);
  
  /*** Write array of x values and vertically averaged values of u ***/
  FILE *averagefile;
  averagefile = fopen("average.dat", "w");
  /* LOOP 11 */
  /* Writing to a file that needs to be plotted, needs to be written in chronological order which parrellelism disrupts */
  float movingSum;
  
  /* Loop through the x's */ 
  for (int i=0; i<NX+2; i++){
    movingSum = 0;
    /* Sum y's to get vertial sum*/ 
    for (int j=0; j<NY+2; j++){
      movingSum += u[i][j];
    }
    float yAvg = movingSum/NY;
    fprintf(averagefile, "%g %g\n", x[i], yAvg);
  }
  fclose(averagefile);

  return 0;
}

/* End of file ******************************************************/