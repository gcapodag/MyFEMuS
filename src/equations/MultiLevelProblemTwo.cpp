/*=========================================================================

 Program: FEMUS
 Module: MultiLevelProblemTwo
 Authors: Giorgio Bornia

 Copyright (c) FEMTTU
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "MultiLevelProblemTwo.hpp"

// C++
#include <iomanip>
#include <sstream>

// local includes 
#include "FemusDefault.hpp"

#include "Files.hpp"
#include "XDMFWriter.hpp"
#include "XDMFWriter.hpp"
#include "Quantity.hpp"
#include "MultiLevelMeshTwo.hpp"
#include "TimeLoop.hpp"

#include "paral.hpp"


namespace femus {



// ====================================================
/// This function constructs the equation map

MultiLevelProblemTwo::MultiLevelProblemTwo(Files& files_in,
                           FemusInputParser<double> & phys_in,
                           QuantityMap& qtymap_in,
                           MultiLevelMeshTwo& mesh_in,
                           std::vector< std::vector<elem_type*> >  & elem_type_in,
			   std::vector<Gauss>   qrule_in ):
        _files(files_in),
        _phys(phys_in),
        _qtymap(qtymap_in),
        _mesh(mesh_in),
        _elem_type(elem_type_in),
        _qrule(qrule_in)  {}


// ====================================================
/// This function destroys the equations
void MultiLevelProblemTwo::clean() {
    for (MultiLevelProblemTwo::iterator eqn = _equations.begin(); eqn != _equations.end(); eqn++) {
        delete eqn->second;
    }
}

// ====================================================
/// This sets dof initial and boundary conditions and sets the operators
//inside this functions there are a lot of new of class elements
//so, i must do a corresponding clean NOT in the DESTRUCTOR
//what is new'd in the constructor is deleted in the destructor
//what is new'd with this function is deleted with a corresponding clear function
//all these things are related to the Dofs, so the first thing is to settle them
// ===============================================================
/// This  function reads all the Operators from files
///  initialization of all levels: dofs and matrices;
/// initialize dof map
/// initialize BC
/// initialize MG Operators (TODO the values of the Restrictor Operators DEPEND on the boundary conditions)
/// initialize vectors (could do this even before the BC)
/// The INITIAL conditions can be done only after initVectors();

void  MultiLevelProblemTwo::setDofBcOpIc() {

    for (iterator eqn = _equations.begin(); eqn != _equations.end(); eqn++) {
        SystemTwo* mgsol = eqn->second;
        
#ifdef DEFAULT_PRINT_INFO
    std::cout << "\n Reading "  <<  mgsol -> _eqname << " Dof, Bc, Op, Ic \n";
#endif

//=====================
    mgsol -> _dofmap.ComputeMeshToDof();
//=====================
    mgsol -> GenerateBdc();
    mgsol -> GenerateBdcElem();
//=====================
    mgsol -> ReadMGOps();
//=====================
    mgsol -> initVectors();     //TODO can I do it earlier than this position?
//=====================
    mgsol -> Initialize();              // initial solution

#ifdef DEFAULT_PRINT_INFO
    std::cout << " Dof, Bc, Op, Ic settled for"  <<  mgsol -> _eqname <<  "\n";
#endif
    }
    
    return;
}






// ========================================================================
/// This function read the solution form all the system (restart)
void MultiLevelProblemTwo::ReadSol(const uint t_step, double& time_out) const {

    const uint ndigits      = DEFAULT_NDIGITS;
    std::string    basesol  = DEFAULT_BASESOL;
    std::string   ext_xdmf  = DEFAULT_EXT_XDMF;
    std::string     ext_h5  = DEFAULT_EXT_H5;
// ---------------------------------------------------
    // reading time from from sol.N.xmf file
    // ---------------------------------------------------
    // open file -----------------------------
    std::ostringstream namefile;
    namefile << _files.GetOutputPath() << "/" 
    << basesol << "." << setw(ndigits) << setfill('0') << t_step << "_l" << (_mesh._NoLevels - 1) << ext_xdmf;  //TODO here we should avoid doing this process TWICE because we already do it in the TransientSetup calling function

#ifdef DEFAULT_PRINT_INFO // --------  info ------------------ 
    std::cout << "\n MultiLevelProblemTwo::read_soln: Reading time  from "
              << namefile.str().c_str();
#endif  // -------------------------------------------
    std::ifstream in ;
    in.open(namefile.str().c_str());  //associate the file stream with the name of the file
    if (!in.is_open()) {
        std::cout << " read_soln: restart .xmf file not found "  << std::endl;
        abort();
    }

    // reading time from xmf file --------------
    std::string buf="";
    while (buf != "<Time") in >> buf;
    in >> buf >> buf;
    buf=buf.substr(2,buf.size()-3);
//create an istringstream from a string
    std::istringstream buffer(buf);
    double restart_time;
    buffer >> restart_time;

    //pass  the time value to the calling function
    time_out = restart_time;

//add parameter to system dont need that now
//   _utils.set_par("restartime",restart_time);

    // ---------------------------------------------------
    // reading data from  sol.N.h5
    // ---------------------------------------------------
    // file name -----------------------------------------
    namefile.str("");  //empty string
    namefile << _files.GetOutputPath() << "/"
    << basesol << "." << setw(ndigits) << setfill('0') << t_step << ext_h5;
    //if i put the path of this file to be relative, will the read depend on where I launched the executable...
    // or where the executable is I think... no, the path is given by where the executable is LAUNCHED

#ifdef DEFAULT_PRINT_INFO  // --------------- info ---------------
    std::cout << "\n MultiLevelProblemTwo::read_soln: Reading from file "
              << namefile.str().c_str() << std::endl;
#endif // ---------------------------------------------
    // loop reading over the variables ---------------------
    for (MultiLevelProblemTwo::const_iterator eqn=_equations.begin(); eqn != _equations.end(); eqn++) {
        SystemTwo *mgsol=eqn->second;
    } //  loop --------------------------------------------------------

    return;
}

// ====================================================
/// This function prints initial and boundary data in xdmf+hdf5 format
// of course whenever you change the fields printed in the case h5 file
// then you need to change also the xdmf file
// we should do a routine that for a given field prints both the hdf5 dataset
// and the  xdmf tag... well it's not so automatic, because you need to know
// what is the grid on which to print, bla bla bla
void MultiLevelProblemTwo::PrintCase(const uint t_init) const {
  
  
  
     PrintCaseHDF5(t_init);
     PrintCaseXDMF(t_init);

    return;
} 


// =============================================================================
/// This function prints initial and boundary data in hdf5 fromat
/// in the file case.h5
void MultiLevelProblemTwo::PrintCaseHDF5(const uint t_init) const {

    const uint    iproc =_mesh._iproc;
    if (iproc==0) {

        const uint ndigits      = DEFAULT_NDIGITS;
        std::string    basecase = DEFAULT_BASECASE;
        std::string   ext_xdmf  = DEFAULT_EXT_XDMF;
        std::string     ext_h5  = DEFAULT_EXT_H5;

        std::ostringstream filename;
        filename << _files.GetOutputPath() << "/" << basecase << "." << setw(ndigits) << setfill('0') << t_init << ext_h5;

        hid_t file = H5Fcreate(filename.str().c_str(),H5F_ACC_TRUNC,H5P_DEFAULT,H5P_DEFAULT);

	for (uint l=0; l< _mesh._NoLevels; l++) {
	
        XDMFWriter::PrintSubdomFlagOnCellsLinear(l,filename.str(),_mesh,LINEAR_FE);

	}
	
        H5Fclose(file);

        MultiLevelProblemTwo::const_iterator pos   = _equations.begin();
        MultiLevelProblemTwo::const_iterator pos_e = _equations.end();
        for (;pos!=pos_e;pos++) {
            SystemTwo* eqn = pos->second;
            XDMFWriter::write_system_solutions(filename.str(),&_mesh,&(eqn->_dofmap),eqn);    // initial solution
            XDMFWriter::write_system_solutions_bc(filename.str(),&_mesh,&(eqn->_dofmap),eqn,eqn->_bc,eqn->_bc_fe_kk);            // boundary condition
        }

    } //end iproc

    return;
}



// ====================================================================
/// It prints the Xdmf file to read the initial and boundary conditions
//now, this function does exactly the same as print sol;
//more over, it prints the PID, and the BOUNDARY CONDITIONS
//so it prints, INITIAL CONDITIONS, BOUNDARY CONDITIONS, PID
//let us split so that we can have a unique function

//clearly, you need to know WHERE to print this file.
//so you need the absolute paths
//but, inside the lines of this file, you dont need to put the absolute paths,
//because you already know you'll not separate .xmf and .h5

void MultiLevelProblemTwo::PrintCaseXDMF(const uint t_init) const {

    const uint    iproc =_mesh._iproc;
    if (iproc==0) {

        const uint NoLevels = _mesh._NoLevels;
        const uint ndigits  = DEFAULT_NDIGITS;

        std::string     basecase = DEFAULT_BASECASE;
        std::string     basemesh = DEFAULT_BASEMESH;
        std::string       ext_h5 = DEFAULT_EXT_H5;
        std::string     ext_xdmf = DEFAULT_EXT_XDMF;
        std::string     aux_xdmf = DEFAULT_AUX_XDMF;
        std::string      connlin = DEFAULT_CONNLIN;
        std::string  bdry_suffix = DEFAULT_BDRY_SUFFIX;
	
        std::ostringstream top_file; top_file << connlin << ext_h5;
        std::ostringstream geom_file; geom_file << basemesh << ext_h5;

        //FE print
        std::string DofType[QL];
        DofType[QQ] = "Node";
        DofType[LL] = "Node";
        DofType[KK] = "Cell";

        std::string var_name[VB];
        std::string var_type[VB];
	
// =================================
// ============= LEVELS ============
// =================================
	
	for (uint l=0; l < NoLevels; l++) {
	  
        std::ostringstream filename_xdmf;
        filename_xdmf << _files.GetOutputPath() << "/"
        << basecase  << "." << setw(ndigits) << setfill('0') << t_init << "_l" << l << ext_xdmf;
        std::ostringstream hdf_file;
        hdf_file <<  basecase << "." << setw(ndigits) << setfill('0') << t_init << ext_h5;

        std::ofstream out(filename_xdmf.str().c_str());
        if (out.fail()) {
            std::cout << "MultiLevelProblemTwo::print_case_xmf: cannot print " << filename_xdmf.str().c_str() << std::endl;
            abort();
        }

        // BEGIN XDMF =======
        out << "<?xml version=\"1.0\" ?> \n";
        out << "<!DOCTYPE Xdmf SYSTEM ";
        out <<  "\"" << _files.GetOutputPath() << "/" << aux_xdmf << "\" \n";
//    out << " [ <!ENTITY HeavyData \"\"> ] ";
        out << ">\n";
        out << "<Xdmf> \n" << "<Domain> \n";
	

        int NGeomObjOnWhichToPrint[QL];
        NGeomObjOnWhichToPrint[QQ] = _mesh._NoNodesXLev[l];
        NGeomObjOnWhichToPrint[LL] = _mesh._NoNodesXLev[l];
        NGeomObjOnWhichToPrint[KK] = _mesh._n_elements_vb_lev[VV][l]*NRE[_mesh._eltype_flag[VV]];

	out << "<Grid Name=\"Volume_L" << l << "\"> \n";

        // TOPOLOGY GEOMETRY ===========
        XDMFWriter::PrintXDMFTopGeomVBLinear(out,top_file,geom_file,l,VV,_mesh);

	// ===== PID ======
        std::ostringstream  pid_name; pid_name << "PID" << "_LEVEL" << l;
	XDMFWriter::PrintXDMFAttribute(out,hdf_file.str(),pid_name.str(),pid_name.str(),"Scalar",DofType[KK],"Int",NGeomObjOnWhichToPrint[KK],1);

        // ATTRIBUTES FOR EACH SYSTEM ===========
        MultiLevelProblemTwo::const_iterator pos1=_equations.begin();
        MultiLevelProblemTwo::const_iterator pos1_e=_equations.end();
        for (;pos1!=pos1_e;pos1++)   {
            SystemTwo *mgsol=pos1->second;
            int OffVarNames[QL];
            OffVarNames[QQ] = 0;
            OffVarNames[LL] = mgsol->_dofmap._nvars[QQ];
            OffVarNames[KK] = mgsol->_dofmap._nvars[QQ] + mgsol->_dofmap._nvars[LL];
            for (int fe=0; fe<QL; fe++)  {
                for (uint ivar=0; ivar < mgsol->_dofmap._nvars[fe]; ivar++)     {
		    std::ostringstream  varstream; varstream << mgsol->_var_names[OffVarNames[fe] + ivar] << "_LEVEL" << l;
                    var_name[VV] = varstream.str();
                    var_type[VV] = "Float";
                    var_name[BB] = var_name[VV] + bdry_suffix;
                    var_type[BB] = "Int";
                    for (int vb=0;vb<VB; vb++) {
                        XDMFWriter::PrintXDMFAttribute(out,hdf_file.str(),var_name[vb],var_name[vb],"Scalar",DofType[fe],var_type[vb],NGeomObjOnWhichToPrint[fe],1);
                    }
                }
            } //end fe

        }  //end eqn

        out << "</Grid>\n";

	out << "</Domain> \n" << "</Xdmf> \n";
        out.close();
	
	} //end levels

    } //if iproc==0
    
    return;
}


} //end namespace femus
