


using namespace femus;

double beta = 0.25;
double Gamma = 0.5;

double gravity[3] = {0., -9.81, 0.};

Line* linea;

void AssembleMPMSys(MultiLevelProblem& ml_prob) {

  // ml_prob is the global object from/to where get/set all the data
  // level is the level of the PDE system to be assembled
  // levelMax is the Maximum level of the MultiLevelProblem
  // assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled

  clock_t AssemblyTime = 0;
  clock_t start_time, end_time;

  //pointers and references

  TransientNonlinearImplicitSystem& my_nnlin_impl_sys = ml_prob.get_system<TransientNonlinearImplicitSystem> ("MPM_FEM");
  const unsigned  level = my_nnlin_impl_sys.GetLevelToAssemble();
  MultiLevelSolution* ml_sol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution* mysolution = ml_sol->GetSolutionLevel(level);     // pointer to the solution (level) object
  LinearEquationSolver* myLinEqSolver = my_nnlin_impl_sys._LinSolver[level];  // pointer to the equation (level) object

  Mesh* mymsh = ml_prob._ml_msh->GetLevel(level);     // pointer to the mesh (level) object
  elem* myel = mymsh->el;   // pointer to the elem object in msh (level)
  SparseMatrix* myKK = myLinEqSolver->_KK;  // pointer to the global stifness matrix object in pdeSys (level)
  NumericVector* myRES =  myLinEqSolver->_RES;  // pointer to the global residual vector object in pdeSys (level)
  bool assembleMatrix = my_nnlin_impl_sys.GetAssembleMatrix();

// call the adept stack object
  adept::Stack& s = FemusInit::_adeptStack;
  if(assembleMatrix) s.continue_recording();
  else s.pause_recording();

  const unsigned dim = mymsh->GetDimension();

  // reserve memory for the local standar vectors
  const unsigned max_size = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  // data
  unsigned iproc  = mymsh->processor_id();

  vector < double > phi;
  vector < double > phi_hat;
  vector < adept::adouble> gradphi;
  vector < double > gradphi_hat;

  phi.reserve(max_size);
  phi_hat.reserve(max_size);

  gradphi.reserve(max_size * dim);
  gradphi_hat.reserve(max_size * dim);

  vector <vector < adept::adouble> > vx(dim); //vx is coordX in assembly of ex30
  vector <vector < double> > vx_hat(dim);

  vector< vector< adept::adouble > > SolDd(dim);      // local solution (displacement)
  vector< vector< double > > SolDdOld(dim);      // local solution (displacement)
  vector< vector< double > > SolVdOld(dim);
  vector< vector< double > > SolAdOld(dim);

  vector< vector< int > > dofsVAR(dim);

  vector< vector< double > > Rhs(dim);     // local redidual vector
  vector< vector< adept::adouble > > aRhs(dim);     // local redidual vector

  vector < int > dofsAll;

  vector < double > Jac;

  adept::adouble weight;
  double weight_hat = 0.;

  //reading parameters for MPM body
  double density_MPM = ml_prob.parameters.get<Solid> ("SolidMPM").get_density();
  double E_MPM = ml_prob.parameters.get<Solid> ("SolidMPM").get_young_module();
  double mu_MPM = ml_prob.parameters.get<Solid> ("SolidMPM").get_lame_shear_modulus();
  double nu_MPM = ml_prob.parameters.get<Solid> ("SolidMPM").get_poisson_coeff();
  double lambda_MPM = ml_prob.parameters.get<Solid> ("SolidMPM").get_lame_lambda();
  double K_MPM = E_MPM / (3.*(1. - 2. * nu_MPM)); //bulk modulus

  //reading parameters for FEM body
  double density_FEM = ml_prob.parameters.get<Solid> ("SolidFEM").get_density();
  double E_FEM = ml_prob.parameters.get<Solid> ("SolidFEM").get_young_module();
  double mu_FEM = ml_prob.parameters.get<Solid> ("SolidFEM").get_lame_shear_modulus();
  double nu_FEM = ml_prob.parameters.get<Solid> ("SolidFEM").get_poisson_coeff();
  double lambda_FEM = ml_prob.parameters.get<Solid> ("SolidFEM").get_lame_lambda();
  double K_FEM = E_FEM / (3.*(1. - 2. * nu_FEM)); //bulk modulus

  double dt =  my_nnlin_impl_sys.GetIntervalTime();

  vector < adept::adouble >* nullAdoublePointer = NULL;
  vector < double >* nullDoublePointer = NULL;

  //variable-name handling
  const char varname[10][5] = {"DX", "DY", "DZ", "VX", "VY", "VZ", "AX", "AY", "AZ", "Mat"};
  vector <unsigned> indexSolD(dim);
  vector <unsigned> indexSolV(dim);
  vector <unsigned> indexSolA(dim);
  vector <unsigned> indexPdeD(dim);
  unsigned solType = ml_sol->GetSolutionType(&varname[0][0]);


  for(unsigned ivar = 0; ivar < dim; ivar++) {
    indexSolD[ivar] = ml_sol->GetIndex(&varname[ivar][0]);
    if(ml_sol->GetIfFSI()){
      indexSolV[ivar] = ml_sol->GetIndex(&varname[ivar + 3][0]);
      indexSolA[ivar] = ml_sol->GetIndex(&varname[ivar + 6][0]);
    }
    indexPdeD[ivar] = my_nnlin_impl_sys.GetSolPdeIndex(&varname[ivar][0]);
  }

  unsigned indexSolMat = ml_sol->GetIndex(&varname[9][0]);
  unsigned solTypeMat = ml_sol->GetSolutionType(&varname[9][0]);

  start_time = clock();

  if(assembleMatrix) myKK->zero();

  //line instances
  std::vector<unsigned> markerOffset = linea->GetMarkerOffset();
  unsigned markerOffset1 = markerOffset[iproc];
  unsigned markerOffset2 = markerOffset[iproc + 1];
  std::vector<Marker*> particles = linea->GetParticles();
  //std::map<unsigned, std::vector < std::vector < std::vector < std::vector < double > > > > > aX;

  //BEGIN loop on elements (to initialize the "soft" stiffness matrix)
  for(int iel = mymsh->_elementOffset[iproc]; iel < mymsh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielt = mymsh->GetElementType(iel);

    int material = mymsh->GetElementMaterial(iel);

    unsigned nDofsD = mymsh->GetElementDofNumber(iel, solType);    // number of solution element dofs
    unsigned nDofs = dim * nDofsD ;//+ nDofsP;
    // resize local arrays
    std::vector <int> sysDof(nDofs);


    for(unsigned  k = 0; k < dim; k++) {
      SolDd[k].resize(nDofsD);
      SolDdOld[k].resize(nDofsD);
      vx[k].resize(nDofsD);
      vx_hat[k].resize(nDofsD);
      if( material == 4 ) {
        SolVdOld[k].resize(nDofsD);
        SolAdOld[k].resize(nDofsD);
      }
    }
    
    
    
    for(unsigned  k = 0; k < dim; k++) {
      aRhs[k].resize(nDofsD);    //resize
      std::fill(aRhs[k].begin(), aRhs[k].end(), 0);    //set aRes to zero
    }

      // local storage of global mapping and solution
    for(unsigned i = 0; i < nDofsD; i++) {
      unsigned idof = mymsh->GetSolutionDof(i, iel, solType);    // global to global mapping between solution node and solution dof
      unsigned idofX = mymsh->GetSolutionDof(i, iel, 2);    // global to global mapping between solution node and solution dof
      
      for(unsigned  k = 0; k < dim; k++) {
        SolDd[k][i] = (*mysolution->_Sol[indexSolD[k]])(idof);      // global extraction and local storage for the solution
	SolDdOld[k][i] = (*mysolution->_SolOld[indexSolD[k]])(idof);      // global extraction and local storage for the solution
        sysDof[i + k * nDofsD] = myLinEqSolver->GetSystemDof(indexSolD[k], indexPdeD[k], i, iel);    // global to global mapping between solution node and pdeSys dof
        vx_hat[k][i] = (*mymsh->_topology->_Sol[k])(idofX);
        vx[k][i] = vx_hat[k][i] + SolDd[k][i];
	if( material == 4 ) {
          SolVdOld[k][i] = (*mysolution->_Sol[indexSolV[k]])(idof);
	  SolAdOld[k][i] = (*mysolution->_Sol[indexSolA[k]])(idof);
	}
      }
    }
	
    // start a new recording of all the operations involving adept::adouble variables
    if(assembleMatrix) s.new_recording();

    // *** Gauss point loop ***
    for(unsigned ig = 0; ig < mymsh->_finiteElement[ielt][solType]->GetGaussPointNumber(); ig++) {


      mymsh->_finiteElement[ielt][solType]->Jacobian(vx_hat, ig, weight_hat, phi_hat, gradphi_hat, *nullDoublePointer);

      vector < vector < adept::adouble > > GradSolDgssHat(dim);

      for(unsigned  k = 0; k < dim; k++) {
        GradSolDgssHat[k].resize(dim);
        std::fill(GradSolDgssHat[k].begin(), GradSolDgssHat[k].end(), 0);
      }

      for(unsigned i = 0; i < nDofsD; i++) {
        for(unsigned j = 0; j < dim; j++) {
          for(unsigned  k = 0; k < dim; k++) {
            GradSolDgssHat[k][j] += gradphi_hat[i * dim + j] * SolDd[k][i];
          }
        }
      }

      if(material == 2) {
        unsigned idofMat = mymsh->GetSolutionDof(0, iel, solTypeMat);
        unsigned  MPMmaterial = (*mysolution->_Sol[indexSolMat])(idofMat);
        double scalingFactor = 0.;// / (1. + 100. * distance);
        if(MPMmaterial == 0) scalingFactor = 1.e-06;
        else if(MPMmaterial == 1) scalingFactor = 5e-03;
        else if(MPMmaterial == 2) scalingFactor = 1e-04;
        for(unsigned i = 0; i < nDofsD; i++) {
          vector < adept::adouble > softStiffness(dim, 0.);

          for(unsigned j = 0; j < dim; j++) {
            for(unsigned  k = 0; k < dim; k++) {
              softStiffness[k]   +=  mu_MPM * gradphi_hat[i * dim + j] * (GradSolDgssHat[k][j] + 0.* GradSolDgssHat[j][k]);
            }
          }
          for(unsigned  k = 0; k < dim; k++) {
            aRhs[k][i] += - softStiffness[k] * weight_hat * scalingFactor;
          }
        }
      }
      else {
	
	mymsh->_finiteElement[ielt][solType]->Jacobian(vx, ig, weight, phi, gradphi, *nullAdoublePointer);
	
	
	vector < adept::adouble > SolDgss(dim, 0);
	vector < double > SolDgssOld(dim, 0);
        vector < double > SolVgssOld(dim, 0);
        vector < double > SolAgssOld(dim, 0);

	vector < vector < adept::adouble > > GradSolDgss(dim);
      

	for(unsigned  k = 0; k < dim; k++) {
	  GradSolDgss[k].resize(dim);
	  std::fill(GradSolDgss[k].begin(), GradSolDgss[k].end(), 0);
	}

	for(unsigned i = 0; i < nDofsD; i++) {
	  for(unsigned  k = 0; k < dim; k++) {
	    SolDgss[k] += phi[i] * SolDd[k][i];
	    SolDgssOld[k] += phi[i] * SolDdOld[k][i];
	    SolVgssOld[k] += phi[i] * SolVdOld[k][i];
	    SolAgssOld[k] += phi[i] * SolAdOld[k][i];
	  }

	  for(unsigned j = 0; j < dim; j++) {
	    for(unsigned  k = 0; k < dim; k++) {
	      GradSolDgss[k][j] += gradphi[i * dim + j] * SolDd[k][i];
	    }
	  }
	}
             
        adept::adouble F[3][3] = {{1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};
        adept::adouble B[3][3];
        adept::adouble Id2th[3][3] = {{ 1., 0., 0.}, { 0., 1., 0.}, { 0., 0., 1.}};
        adept::adouble Cauchy[3][3];

        for(int i = 0; i < dim; i++) {
          for(int j = 0; j < dim; j++) {
            F[i][j] += GradSolDgssHat[i][j];
          }
        }

        adept::adouble J_hat =  F[0][0] * F[1][1] * F[2][2] + F[0][1] * F[1][2] * F[2][0] + F[0][2] * F[1][0] * F[2][1]
                                - F[2][0] * F[1][1] * F[0][2] - F[2][1] * F[1][2] * F[0][0] - F[2][2] * F[1][0] * F[0][1];

        for(int i = 0; i < 3; i++) {
          for(int j = 0; j < 3; j++) {
            B[i][j] = 0.;

            for(int k = 0; k < 3; k++) {
              //left Cauchy-Green deformation tensor or Finger tensor (B = F*F^T)
              B[i][j] += F[i][k] * F[j][k];
            }
          }
        }

        adept::adouble I1_B = B[0][0] + B[1][1] + B[2][2];

        for(int i = 0; i < 3; i++) {
          for(int j = 0; j < 3; j++) {
            //  Cauchy[i][j] = mu * (B[i][j] - I1_B * Id2th[i][j] / 3.) / pow(J_hat, 5. / 3.)
            //                 + K * (J_hat - 1.) * Id2th[i][j];  //Generalized Neo-Hookean solid, in Allan-Bower's book, for rubbers with very limited compressibility and K >> mu

            Cauchy[i][j] = lambda_FEM * log(J_hat) / J_hat * Id2th[i][j] + mu_FEM / J_hat * (B[i][j] - Id2th[i][j]); //alternative formulation

          }
        }

        for(unsigned i = 0; i < nDofsD; i++) {
          adept::adouble CauchyDIR[3] = {0., 0., 0.};

          for(int idim = 0.; idim < dim; idim++) {
            for(int jdim = 0.; jdim < dim; jdim++) {
              CauchyDIR[idim] += gradphi[i * dim + jdim] * Cauchy[idim][jdim];
            }
          }

          for(int idim = 0; idim < dim; idim++) {
            aRhs[indexPdeD[idim]][i] += (phi[i] * density_FEM / J_hat * gravity[idim] - CauchyDIR[idim]
                                        -phi[i] * ( 1. / (beta * dt * dt) * (SolDgss[idim]-SolDgssOld[idim])
                                                   -1. / (beta * dt) * SolVgssOld[idim] 
                                                   -(1. - 2.* beta) / (2. * beta) * SolAgssOld[idim] ) 
					) * weight;
          }
        }
      }
    } // end gauss point loop


    //copy the value of the adept::adoube aRes in double Res and store them in RES
    std::vector<double> Rhs(nDofs);  //resize

    for(int i = 0; i < nDofsD; i++) {
      for(unsigned  k = 0; k < dim; k++) {
        Rhs[ i +  k * nDofsD ] = -aRhs[k][i].value();
      }
    }

    myRES->add_vector_blocked(Rhs, sysDof);

    if(assembleMatrix) {
      Jac.resize(nDofs * nDofs);
      // define the dependent variables

      for(unsigned  k = 0; k < dim; k++) {
        s.dependent(&aRhs[k][0], nDofsD);
      }

      // define the independent variables
      for(unsigned  k = 0; k < dim; k++) {
        s.independent(&SolDd[k][0], nDofsD);
      }

      // get the and store jacobian matrix (row-major)
      s.jacobian(&Jac[0] , true);
      myKK->add_matrix_blocked(Jac, sysDof, sysDof);

      s.clear_independents();
      s.clear_dependents();
    }
  }
  //END building "soft" stiffness matrix


  //initialization of iel
  unsigned ielOld = UINT_MAX;

  //BEGIN loop on particles (used as Gauss points)
  for(unsigned iMarker = markerOffset1; iMarker < markerOffset2; iMarker++) {

    //element of particle iMarker
    unsigned iel = particles[iMarker]->GetMarkerElement();
    if(iel != UINT_MAX) {
      short unsigned ielt;
      unsigned nDofsD;
      
      //BEGIN considering traction
      
      double traction[3] = {0., 0., 0.};
      double boundaryLayer = 0.0542;
      
      //END
      
      
      //update element related quantities only if we are in a different element
      if(iel != ielOld) {

        ielt = mymsh->GetElementType(iel);
        nDofsD = mymsh->GetElementDofNumber(iel, solType);
        //initialization of everything is in common fluid and solid

        //Rhs
        for(int i = 0; i < dim; i++) {
          dofsVAR[i].resize(nDofsD);
          SolDd[indexPdeD[i]].resize(nDofsD);
	  SolDdOld[indexPdeD[i]].resize(nDofsD);
          aRhs[indexPdeD[i]].resize(nDofsD);
          vx[i].resize(nDofsD);
          vx_hat[i].resize(nDofsD);
        }
        dofsAll.resize(0);


        //BEGIN copy of the value of Sol at the dofs idof of the element iel
        for(unsigned i = 0; i < nDofsD; i++) {
          unsigned idof = mymsh->GetSolutionDof(i, iel, solType); //local 2 global solution
          unsigned idofX = mymsh->GetSolutionDof(i, iel, 2); //local 2 global coordinates

	  if( mymsh->GetSolidMark(idof) ) traction[0] = gravity[0];
	  
	  
          for(int j = 0; j < dim; j++) {
            SolDd[indexPdeD[j]][i] = (*mysolution->_Sol[indexSolD[j]])(idof);
	    SolDdOld[indexPdeD[j]][i] = (*mysolution->_SolOld[indexSolD[j]])(idof);

            dofsVAR[j][i] = myLinEqSolver->GetSystemDof(indexSolD[j], indexPdeD[j], i, iel); //local 2 global Pde
            aRhs[indexPdeD[j]][i] = 0.;

            //Fixed coordinates (Reference frame)
            vx_hat[j][i] = (*mymsh->_topology->_Sol[j])(idofX);
            // vx[j][i]    = vx_hat[j][i] + 0.5 * SolDd[indexPdeD[j]][i];
            vx[j][i]    = vx_hat[j][i] + SolDd[indexPdeD[j]][i];
          }
        }
        //END

        // build dof composition
        for(int idim = 0; idim < dim; idim++) {
          dofsAll.insert(dofsAll.end(), dofsVAR[idim].begin(), dofsVAR[idim].end());
        }

        if(assembleMatrix) s.new_recording();

        // start a new recording of all the operations involving adept::adouble variables
      }

      //bool elementUpdate = (aX.find(iel) != aX.end()) ? false : true;  //TODO to be removed after we include FindLocalCoordinates in the advection
      //particles[iMarker]->FindLocalCoordinates(solType, aX[iel], elementUpdate, mysolution, 0);

      // the local coordinates of the particles are the Gauss points in this context
      std::vector <double> xi = particles[iMarker]->GetMarkerLocalCoordinates();

//       if(iMarker < 3){
// 	std::cout << iel <<" "<< iMarker << " " << xi[0] << " " << xi[1] <<std::endl;
//       }

      mymsh->_finiteElement[ielt][solType]->Jacobian(vx, xi, weight, phi, gradphi, *nullAdoublePointer); //function to evaluate at the particles
      mymsh->_finiteElement[ielt][solType]->Jacobian(vx_hat, xi, weight_hat, phi_hat, gradphi_hat, *nullDoublePointer);


      // displacement and velocity
      //BEGIN evaluates SolDp at the particle iMarker
      vector<adept::adouble> SolDp(dim,0.);
      vector<double> SolDpOld(dim,0.);
      vector<vector < adept::adouble > > GradSolDp(dim);
      vector<vector < adept::adouble > > GradSolDpHat(dim);
      vector < vector < adept::adouble > > LocalFp(dim);

      for(int i = 0; i < dim; i++) {
        GradSolDp[i].resize(dim);
        std::fill(GradSolDp[i].begin(), GradSolDp[i].end(), 0);
        GradSolDpHat[i].resize(dim);
        std::fill(GradSolDpHat[i].begin(), GradSolDpHat[i].end(), 0);
      }

      for(int i = 0; i < dim; i++) {
        for(unsigned inode = 0; inode < nDofsD; inode++) {
          SolDp[i] += phi[inode] * SolDd[indexPdeD[i]][inode];
	  SolDpOld[i] += phi[inode] * SolDdOld[indexPdeD[i]][inode];
          for(int j = 0; j < dim; j++) {
            GradSolDp[i][j] +=  gradphi[inode * dim + j] * SolDd[indexPdeD[i]][inode];
//             GradSolDpHat[i][j] +=  gradphi_hat[inode * dim + j] * 0.5 * SolDd[indexPdeD[i]][inode];
            GradSolDpHat[i][j] +=  gradphi_hat[inode * dim + j] * (SolDd[indexPdeD[i]][inode] - SolDdOld[indexPdeD[i]][inode]);
          }
        }
      }
      //END evaluates SolDp at the particle iMarker

      std::vector <double> SolVpOld(dim);
      particles[iMarker]->GetMarkerVelocity(SolVpOld);


      std::vector <double> SolApOld(dim);
      particles[iMarker]->GetMarkerAcceleration(SolApOld);

      double mass = particles[iMarker]->GetMarkerMass();

      //BEGIN computation of the Cauchy Stress
      std::vector < std::vector < double > > FpOld;
      FpOld = particles[iMarker]->GetDeformationGradient(); //extraction of the deformation gradient

      adept::adouble FpNew[3][3] = {{1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};
      adept::adouble F[3][3] = {{0., 0., 0.}, {0., 0., 0.}, {0., 0., 0.}};
      adept::adouble B[3][3];
      adept::adouble Id2th[3][3] = {{ 1., 0., 0.}, { 0., 1., 0.}, { 0., 0., 1.}};
      adept::adouble Cauchy[3][3];

      for(int i = 0; i < dim; i++) {
        for(int j = 0; j < dim; j++) {
          FpNew[i][j] += GradSolDpHat[i][j];
        }
      }

      for(int i = 0; i < dim; i++) {
        for(int j = 0; j < dim; j++) {
          for(int k = 0; k < dim; k++) {
            F[i][j] += FpNew[i][k] * FpOld[k][j];
          }
        }
      }

      if(dim == 2) F[2][2] = 1.;

      adept::adouble J_hat =  F[0][0] * F[1][1] * F[2][2] + F[0][1] * F[1][2] * F[2][0] + F[0][2] * F[1][0] * F[2][1]
                              - F[2][0] * F[1][1] * F[0][2] - F[2][1] * F[1][2] * F[0][0] - F[2][2] * F[1][0] * F[0][1];

      for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
          B[i][j] = 0.;

          for(int k = 0; k < 3; k++) {
            //left Cauchy-Green deformation tensor or Finger tensor (B = F*F^T)
            B[i][j] += F[i][k] * F[j][k];
          }
        }
      }

      adept::adouble I1_B = B[0][0] + B[1][1] + B[2][2];

      for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
          //Cauchy[i][j] = mu_MPM * (B[i][j] - I1_B * Id2th[i][j] / 3.) / pow(J_hat, 5. / 3.)
            //             + K_MPM * (J_hat - 1.) * Id2th[i][j];  //Generalized Neo-Hookean solid, in Allan-Bower's book, for rubbers with very limited compressibility and K >> mu

	  Cauchy[i][j] = lambda_MPM * log(J_hat) / J_hat * Id2th[i][j] + mu_MPM / J_hat * (B[i][j] - Id2th[i][j]); //alternative formulation


        }
      }
      //END computation of the Cauchy Stress

      //BEGIN redidual Solid Momentum in moving domain
      for(unsigned i = 0; i < nDofsD; i++) {
        adept::adouble CauchyDIR[3] = {0., 0., 0.};

        for(int idim = 0.; idim < dim; idim++) {
          for(int jdim = 0.; jdim < dim; jdim++) {
            CauchyDIR[idim] += gradphi[i * dim + jdim] * Cauchy[idim][jdim];
          }
        }

        for(int idim = 0; idim < dim; idim++) {
          aRhs[indexPdeD[idim]][i] += (phi[i] * gravity[idim] - J_hat * CauchyDIR[idim] / density_MPM /*+ phi[i] * 1. / (boundaryLayer * 3. * density_MPM) * traction[idim]*/ 
//                                     -  phi[i] * 0.5 * (SolApOld[idim] + 1. / (beta * dt * dt) * SolDp[idim] - 1. / (beta * dt) * SolVpOld[idim] - (1. - 2.* beta) / (2. * beta) * SolApOld[idim])
                                       -  phi[i] * (1. / (beta * dt * dt) * (SolDp[idim]-SolDpOld[idim]) - 1. / (beta * dt) * SolVpOld[idim] - (1. - 2.* beta) / (2. * beta) * SolApOld[idim])
                                      ) * mass;
        }
      }
      //END redidual Solid Momentum in moving domain


      if(iMarker == markerOffset2 - 1 || iel != particles[iMarker + 1]->GetMarkerElement()) {

        //copy adouble aRhs into double Rhs
        for(unsigned i = 0; i < dim; i++) {
          Rhs[indexPdeD[i]].resize(nDofsD);

          for(int j = 0; j < nDofsD; j++) {
            Rhs[indexPdeD[i]][j] = -aRhs[indexPdeD[i]][j].value();
          }
        }

        for(int i = 0; i < dim; i++) {
          myRES->add_vector_blocked(Rhs[indexPdeD[i]], dofsVAR[i]);
        }

        if(assembleMatrix) {
          //Store equations
          for(int i = 0; i < dim; i++) {
            s.dependent(&aRhs[indexPdeD[i]][0], nDofsD);
            s.independent(&SolDd[indexPdeD[i]][0], nDofsD);
          }

          Jac.resize((dim * nDofsD) * (dim * nDofsD));

          s.jacobian(&Jac[0], true);

          myKK->add_matrix_blocked(Jac, dofsAll, dofsAll);
          s.clear_independents();
          s.clear_dependents();
        }
      }
      //END local to global assembly

      ielOld = iel;
    }
    else {
      break;
    }
  }
  //END loop on particles

  myRES->close();
  mysolution->_Sol[indexSolMat]->close();

  if(assembleMatrix) {
    myKK->close();
  }

  // *************************************
  end_time = clock();
  AssemblyTime += (end_time - start_time);
  // ***************** END ASSEMBLY RESIDUAL + MATRIX *******************

}


void GridToParticlesProjection(MultiLevelProblem & ml_prob, Line & linea) {

  // ml_prob is the global object from/to where get/set all the data
  // level is the level of the PDE system to be assembled
  // levelMax is the Maximum level of the MultiLevelProblem
  // assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled

  clock_t AssemblyTime = 0;
  clock_t start_time, end_time;

  //pointers and references

  TransientNonlinearImplicitSystem& my_nnlin_impl_sys = ml_prob.get_system<TransientNonlinearImplicitSystem> ("MPM_FEM");
  //NonLinearImplicitSystem& my_nnlin_impl_sys = ml_prob.get_system<NonLinearImplicitSystem> ("MPM_FEM");
  const unsigned  level = my_nnlin_impl_sys.GetLevelToAssemble();
  MultiLevelSolution* ml_sol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution* mysolution = ml_sol->GetSolutionLevel(level);     // pointer to the solution (level) object

  Mesh* mymsh = ml_prob._ml_msh->GetLevel(level);     // pointer to the mesh (level) object
  elem* myel = mymsh->el;   // pointer to the elem object in msh (level)

  double dt =  my_nnlin_impl_sys.GetIntervalTime();

  const unsigned dim = mymsh->GetDimension();

  // data
  unsigned iproc  = mymsh->processor_id();

  // local objects
  vector< vector < double > > SolDd(dim);
  vector< vector < double > > SolDdOld(dim);
  vector< vector < double > > GradSolDp(dim);

  for(int i = 0; i < dim; i++) {
    GradSolDp[i].resize(dim);
  }

  vector < double > phi;
  vector < double > gradphi;
  vector < double > nablaphi;

  vector <vector < double> > vx(dim); //vx is coordX in assembly of ex30

  double weight;

  //variable-name handling
  const char varname[9][3] = {"DX", "DY", "DZ", "VX", "VY", "VW", "AX", "AY", "AW"};
  vector <unsigned> indexSolD(dim);
  vector <unsigned> indexSolV(dim);
  vector <unsigned> indexSolA(dim);
  unsigned solType = ml_sol->GetSolutionType(&varname[0][0]);

  for(unsigned ivar = 0; ivar < dim; ivar++) {
    indexSolD[ivar] = ml_sol->GetIndex(&varname[ivar][0]);
    if(ml_sol->GetIfFSI()){
      indexSolV[ivar] = ml_sol->GetIndex(&varname[ivar + 3][0]);
      indexSolA[ivar] = ml_sol->GetIndex(&varname[ivar + 6][0]);
    }
  }

  //line instances
  std::vector<unsigned> markerOffset = linea.GetMarkerOffset();
  unsigned markerOffset1 = markerOffset[iproc];
  unsigned markerOffset2 = markerOffset[iproc + 1];
  std::vector<Marker*> particles = linea.GetParticles();
  //std::map<unsigned, std::vector < std::vector < std::vector < std::vector < double > > > > > aX;

  //initialization of iel
  unsigned ielOld = UINT_MAX;
  //declaration of element instances

  //BEGIN loop on particles
  for(unsigned iMarker = markerOffset1; iMarker < markerOffset2; iMarker++) {

    //element of particle iMarker
    unsigned iel = particles[iMarker]->GetMarkerElement();


    if(iel != UINT_MAX) {

      short unsigned ielt;
      unsigned nve;

      //update element related quantities only if we are in a different element
      if(iel != ielOld) {

        ielt = mymsh->GetElementType(iel);
        nve = mymsh->GetElementDofNumber(iel, solType);

        for(int i = 0; i < dim; i++) {
          SolDd[i].resize(nve);
	  SolDdOld[i].resize(nve);
          vx[i].resize(nve);
        }

        //BEGIN copy of the value of Sol at the dofs idof of the element iel
        for(unsigned inode = 0; inode < nve; inode++) {
          unsigned idof = mymsh->GetSolutionDof(inode, iel, solType); //local 2 global solution
          unsigned idofX = mymsh->GetSolutionDof(inode, iel, 2); //local 2 global solution

          for(int i = 0; i < dim; i++) {
            SolDd[i][inode] = (*mysolution->_Sol[indexSolD[i]])(idof);
	    SolDdOld[i][inode] = (*mysolution->_SolOld[indexSolD[i]])(idof);
            //moving domain
            vx[i][inode] = (*mymsh->_topology->_Sol[i])(idofX);
          }
        }
        //END
      }
      std::vector <double> xi = particles[iMarker]->GetMarkerLocalCoordinates();
      
      //bool elementUpdate = (aX.find(iel) != aX.end()) ? false : true;  //TODO to be removed after we include FindLocalCoordinates in the advection
      //particles[iMarker]->FindLocalCoordinates(solType, aX[iel], elementUpdate, mysolution, 0);
      // xi = particles[iMarker]->GetMarkerLocalCoordinates();

      mymsh->_finiteElement[ielt][solType]->Jacobian(vx, xi, weight, phi, gradphi, nablaphi); //function to evaluate at the particles

      std::vector <double> particleDisp(dim, 0.);
      std::vector <double> particleDispNew(dim, 0.);
      //update displacement and acceleration
      for(int i = 0; i < dim; i++) {
        for(unsigned inode = 0; inode < nve; inode++) {
	  //std::cout << SolDd[i][inode] << " ";
          particleDisp[i] += phi[inode] * (SolDd[i][inode] - SolDdOld[i][inode]);
	  particleDispNew[i] += phi[inode] * SolDd[i][inode];
	  //std::cout<<std::endl;
        }
        //std::cout << particleDisp[i] <<std::endl;
      }
      

      particles[iMarker]->SetMarkerDisplacement(particleDisp);
      particles[iMarker]->UpdateParticleCoordinates();


      std::vector <double> particleVelOld(dim);
      particles[iMarker]->GetMarkerVelocity(particleVelOld);

      std::vector <double> particleAccOld(dim);
      particles[iMarker]->GetMarkerAcceleration(particleAccOld);

      std::vector <double> particleAcc(dim);
      std::vector <double> particleVel(dim);
      for(unsigned i = 0; i < dim; i++) {
        particleAcc[i] = 1. / (beta * dt * dt) * particleDisp[i] - 1. / (beta * dt) * particleVelOld[i] - (1. - 2.* beta) / (2. * beta) * particleAccOld[i];
        //particleVel[i] = 2. / dt * particleDisp[i] - particleVelOld[i];
        particleVel[i] = particleVelOld[i] + dt * ((1. - Gamma) * particleAccOld[i] + Gamma * particleAcc[i]);
      }

      particles[iMarker]->SetMarkerVelocity(particleVel);
      particles[iMarker]->SetMarkerAcceleration(particleAcc);

      //   update the deformation gradient
      for(int i = 0; i < dim; i++) {
        for(int j = 0; j < dim; j++) {
          GradSolDp[i][j] = 0.;
          for(unsigned inode = 0; inode < nve; inode++) {
            GradSolDp[i][j] +=  gradphi[inode * dim + j] * (SolDd[i][inode] - SolDdOld[i][inode]);
          }
        }
      }

      std::vector < std::vector < double > > FpOld;
      FpOld = particles[iMarker]->GetDeformationGradient(); //extraction of the deformation gradient

      double FpNew[3][3] = {{1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};
      std::vector < std::vector < double > > Fp(dim);

      for(unsigned i = 0; i < dim; i++) {
        for(unsigned j = 0; j < dim; j++) {
          FpNew[i][j] += GradSolDp[i][j];
        }
      }

      for(unsigned i = 0; i < dim; i++) {
        Fp[i].resize(dim);
        for(unsigned j = 0; j < dim; j++) {
          Fp[i][j] = 0.;
          for(unsigned k = 0; k < dim; k++) {
            Fp[i][j] += FpNew[i][k] * FpOld[k][j];
          }
        }
      }

      particles[iMarker]->SetDeformationGradient(Fp);

      ielOld = iel;
    }
    else {
      break;
    }
  }
  //END loop on particles


  //BEGIN loop on elements to update grid velocity and acceleration
  for (unsigned idof = mymsh->_dofOffset[solType][iproc]; idof < mymsh->_dofOffset[solType][iproc + 1]; idof++) {
    
    if( mymsh->GetSolidMark(idof) ){
    
      for(int i = 0; i < dim; i++) {
	double disp = (*mysolution->_Sol[indexSolD[i]])(idof);
	double velOld = (*mysolution->_Sol[indexSolV[i]])(idof);
	double accOld = (*mysolution->_Sol[indexSolA[i]])(idof);
	double accNew = 1. / (beta * dt * dt) * disp - 1. / (beta * dt) * velOld - (1. - 2.* beta) / (2. * beta) * accOld;
	double velNew = velOld + dt * ((1. - Gamma) * accOld + Gamma * accNew);
	mysolution->_Sol[indexSolV[i]]->set(idof, velNew);
	mysolution->_Sol[indexSolA[i]]->set(idof, accNew);
      }
    }
    else{
      for(int i = 0; i < dim; i++) {
	mysolution->_Sol[indexSolD[i]]->set(idof, 0.);
      }
    }
  }
  
  for(int i = 0; i < dim; i++) {
    mysolution->_Sol[indexSolD[i]]->close();
    if( ml_sol->GetIfFSI() ){    
      mysolution->_Sol[indexSolV[i]]->close();
      mysolution->_Sol[indexSolA[i]]->close();
    }
  }
  //END loop on elements to update grid velocity and acceleration
  
  linea.UpdateLineMPM();

  linea.GetParticlesToGridMaterial();
}



