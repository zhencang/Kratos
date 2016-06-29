//
//   Project Name:        KratosPfemBaseApplication $
//   Created by:          $Author:      JMCarbonell $
//   Last modified by:    $Co-Author:               $
//   Date:                $Date:      February 2016 $
//   Revision:            $Revision:            0.0 $
//
//

#if !defined( KRATOS_SELECT_MESH_ELEMENTS_PROCESS_H_INCLUDED )
#define KRATOS_SELECT_MESH_ELEMENTS_PROCESS_H_INCLUDED


// External includes

// System includes

// Project includes
#include "containers/variables_list_data_value_container.h"
#include "spatial_containers/spatial_containers.h"

#include "includes/model_part.h"
#include "custom_utilities/modeler_utilities.hpp"
#include "geometries/tetrahedra_3d_4.h"

///VARIABLES used:
//Data:     
//StepData: NODAL_H, CONTACT_FORCE
//Flags:    (checked) TO_REFOME, BOUNDARY, NEW_ENTITY
//          (set)     
//          (modified)  
//          (reset)   
//(set):=(set in this process)

namespace Kratos
{

///@name Kratos Classes
///@{

/// Refine Mesh Elements Process 2D and 3D
/** The process labels the elements to be refined in the mesher
    it applies a size constraint to elements that must be refined.
    
*/
class SelectMeshElementsProcess
  : public Process
{
public:
    ///@name Type Definitions
    ///@{

    /// Pointer definition of Process
    KRATOS_CLASS_POINTER_DEFINITION( SelectMeshElementsProcess );

    typedef ModelPart::ConditionType         ConditionType;
    typedef ModelPart::PropertiesType       PropertiesType;
    typedef ConditionType::GeometryType       GeometryType;

    ///@}
    ///@name Life Cycle
    ///@{

    /// Default constructor.
    SelectMeshElementsProcess(ModelPart& rModelPart,
			      ModelerUtilities::MeshingParameters& rRemeshingParameters,
			      ModelPart::IndexType MeshId,
			      int EchoLevel) 
      : mrModelPart(rModelPart),
	mrRemesh(rRemeshingParameters)
    {
    
      mMeshId = MeshId;
      mEchoLevel = EchoLevel;
    }


    /// Destructor.
    virtual ~SelectMeshElementsProcess() {}


    ///@}
    ///@name Operators
    ///@{

    /// This operator is provided to call the process as a function and simply calls the Execute method.
    void operator()()
    {
        Execute();
    }


    ///@}
    ///@name Operations
    ///@{


    /// Execute method is used to execute the Process algorithms.
    virtual void Execute()
    {
      KRATOS_TRY

      if( mEchoLevel > 0 )
	std::cout<<" [ SELECT MESH ELEMENTS: ("<<(mrRemesh.OutMesh.NumberOfElements)<<") "<<std::endl;

      mrRemesh.PreservedElements.clear();
      mrRemesh.PreservedElements.resize(mrRemesh.OutMesh.NumberOfElements);
      std::fill( mrRemesh.PreservedElements.begin(), mrRemesh.PreservedElements.end(), 0 );
      
      mrRemesh.Info->NumberOfElements=0;
    
      bool box_side_element = false;
      bool wrong_added_node = false;


      if(mrRemesh.ExecutionOptions.IsNot(ModelerUtilities::SELECT_ELEMENTS))
	{
	  for(int el=0; el<mrRemesh.OutMesh.NumberOfElements; el++)
	    {
	      mrRemesh.PreservedElements[el]=1;
	      mrRemesh.Info->NumberOfElements+=1;
	    }
	}
      else
	{
	  if( mEchoLevel > 0 )
	    std::cout<<"   Start Element Selection "<<mrRemesh.OutMesh.NumberOfElements<<std::endl;

	  ModelPart::ElementsContainerType::iterator element_begin = mrModelPart.ElementsBegin(mMeshId);	  
	  const unsigned int nds = (*element_begin).GetGeometry().size();
	  const unsigned int dimension = element_begin->GetGeometry().WorkingSpaceDimension();

	  int* OutElementList = mrRemesh.OutMesh.GetElementList();
	 
	  ModelPart::NodesContainerType& rNodes = mrModelPart.Nodes(mMeshId);

	  int el;
	  int number=0;
	  //#pragma omp parallel for reduction(+:number) private(el)
	  for(el=0; el<mrRemesh.OutMesh.NumberOfElements; el++)
	    {
	      Geometry<Node<3> > vertices;
	      //double Alpha   = 0;
	      //double nodal_h = 0;
	      //double param   = 0.3333333;
	      
	      // int  numflying=0;
	      // int  numlayer =0;
	      //int  numfixed =0;
	      
	      unsigned int  numfreesurf =0;
	      unsigned int  numboundary =0;
	      
	      // std::cout<<" num nodes "<<rNodes.size()<<std::endl;
	      // std::cout<<" selected vertices [ "<<OutElementList[el*3]<<", "<<OutElementList[el*3+1]<<", "<<OutElementList[el*3+2]<<"] "<<std::endl;
	      box_side_element = false;
	      for(unsigned int pn=0; pn<nds; pn++)
		{
		  //set vertices
		  if(mrRemesh.NodalPreIds[OutElementList[el*nds+pn]]<0){
		    if(mrRemesh.Options.IsNot(ModelerUtilities::CONTACT_SEARCH))
		      std::cout<<" ERROR: something is wrong: nodal id < 0 "<<std::endl;
		    box_side_element = true;
		    break;
		  }
		
		  
		  if( (unsigned int)OutElementList[el*nds+pn] > mrRemesh.NodalPreIds.size() ){
		    wrong_added_node = true;
		    std::cout<<" ERROR: something is wrong: node out of bounds "<<std::endl;
		    break;
		  }
		
		  //vertices.push_back( *((rNodes).find( OutElementList[el*nds+pn] ).base() ) );
		  vertices.push_back(rNodes(OutElementList[el*nds+pn]));

		  //check flags on nodes
		  if(vertices.back().Is(FREE_SURFACE))
		    numfreesurf++;

		  if(vertices.back().Is(BOUNDARY))
		    numboundary++;
		  
		  // if(VertexPa[pn].match(_wall_))
		  // 	numfixed++;
		  
		  // if(VertexPa[pn].match(_flying_))
		  // 	numflying++;
		  
		  // if(VertexPa[pn].match(_layer_))
		  // 	numlayer++;
		  
		  //nodal_h+=vertices.back().FastGetSolutionStepValue(NODAL_H);
		  
		}
	      
	      
	      if(box_side_element || wrong_added_node){
		//std::cout<<" Box_Side_Element "<<std::endl;
		continue;
	      }
	      
	      //1.- to not consider wall elements
	      // if(numfixed==3)
	      //   Alpha=0;
	      
	      //2.- alpha shape:
	      //Alpha  = nodal_h * param;
	      //Alpha *= mrRemesh.AlphaParameter; //1.4; 1.35;
	      
	      //2.1.- correction to avoid big elements on boundaries
	      // if(numflying>0){
	      //   Alpha*=0.8;
	      // }
	      // else{
	      //   if(numfixed+numsurf<=2){
	      //     //2.2.- correction to avoid voids in the fixed boundaries
	      //     if(numfixed>0)
	      // 	Alpha*=1.4;
	      
	      //     //2.3.- correction to avoid voids on the free surface
	      //     if(numsurf>0)
	      // 	Alpha*=1.3;
	      
	      //     //2.4.- correction to avoid voids in the next layer after fixed boundaries
	      //     if(numlayer>0 && !numsurf)
	      // 	Alpha*=1.2;
	      //   }
	      
	      // }
	      
	      //std::cout<<" ******** ELEMENT "<<el+1<<" ********** "<<std::endl;
	      
	      double Alpha =  mrRemesh.AlphaParameter; //*nds;
	      if(numboundary>=nds-1)
		Alpha*=1.8;
	
	      // std::cout<<" vertices for the contact element "<<std::endl;
	      // for( unsigned int n=0; n<nds; n++)
	      // 	{
	      // 	  std::cout<<" ("<<n+1<<"): ["<<mrRemesh.NodalPreIds[vertices[n].Id()]<<"] "<<vertices[n]<<std::endl;
	      // 	}

	      // std::cout<<" vertices for the subdomain element "<<std::endl;
	      // for( unsigned int n=0; n<nds; n++)
	      // 	{
	      // 	  std::cout<<" ("<<n+1<<"): ["<<vertices[n].Id()<<"] "<<vertices[n]<<std::endl;
	      // 	}
	      
	      // std::cout<<" Element "<<el<<" with alpha "<<mrRemesh.AlphaParameter<<"("<<Alpha<<")"<<std::endl;
	      
	      bool accepted=false;
	      
	      ModelerUtilities ModelerUtils;
	      if(mrRemesh.ExecutionOptions.Is(ModelerUtilities::PASS_ALPHA_SHAPE)){
		
		if(mrRemesh.Options.Is(ModelerUtilities::CONTACT_SEARCH))
		  {
		    accepted=ModelerUtils.ShrankAlphaShape(Alpha,vertices,mrRemesh.OffsetFactor,dimension);
		  }
		else
		  {
		    accepted=ModelerUtils.AlphaShape(Alpha,vertices,dimension);
		  }

	      }
	      else{

		accepted = true;

	      }
	      	   
	      //3.- to control all nodes from the same subdomain (problem, domain is not already set for new inserted particles on mesher)
	      // if(accepted)
	      // {
	      //   std::cout<<" Element passed Alpha Shape "<<std::endl;
	      //     if(mrRemesh.Refine->Options.IsNot(ModelerUtilities::CONTACT_SEARCH))
	      //   	accepted=ModelerUtilities::CheckSubdomain(vertices);
	      // }

	      //3.1.-
	      bool self_contact = false;
	      if(mrRemesh.Options.Is(ModelerUtilities::CONTACT_SEARCH))
		self_contact = ModelerUtils.CheckSubdomain(vertices);
	    	    
	      //4.- to control that the element is inside of the domain boundaries
	      if(accepted)
		{
		  if(mrRemesh.Options.Is(ModelerUtilities::CONTACT_SEARCH))
		    {
		      accepted=ModelerUtils.CheckOuterCentre(vertices,mrRemesh.OffsetFactor, self_contact);
		    }
		  else
		    {
		      //accepted=ModelerUtils.CheckInnerCentre(vertices); //problems in 3D: when slivers are released, a boundary is created and the normals calculated, then elements that are inside suddently its center is calculated as outside... // some corrections are needded.
	
		    }
		}
	      // else{
	      
	      //   std::cout<<" Element DID NOT pass Alpha Shape ("<<Alpha<<") "<<std::endl;
	      // }
	      
	      //5.- to control that the element has a good shape
	      int sliver = 0;
	      if(accepted)
		{
		  if(nds==4){
		    Geometry<Node<3> >* tetrahedron = new Tetrahedra3D4<Node<3> > (vertices);

		    accepted=ModelerUtils.CheckGeometryShape(*tetrahedron,sliver);
		
		    if( sliver )
		      accepted = false;

		    delete tetrahedron;
		  }
		}


	      if(accepted)
		{
		  //std::cout<<" Element ACCEPTED after cheking Center "<<number<<std::endl;
		  mrRemesh.PreservedElements[el] = 1;
		  number+=1;
		}
	      // else{
	      
	      //   std::cout<<" Element DID NOT pass INNER/OUTER check "<<std::endl;
	      // }


	    }

	  mrRemesh.Info->NumberOfElements=number;

	}

      //std::cout<<"   Number of Preserved Elements "<<mrRemesh.Info->NumberOfElements<<std::endl;

      if(mrRemesh.ExecutionOptions.Is(ModelerUtilities::ENGAGED_NODES)){


	ModelPart::ElementsContainerType::iterator element_begin = mrModelPart.ElementsBegin(mMeshId);	  
	const unsigned int nds = (*element_begin).GetGeometry().size();
      
	int* OutElementList = mrRemesh.OutMesh.GetElementList();
      
	ModelPart::NodesContainerType& rNodes = mrModelPart.Nodes(mMeshId);

	//check engaged nodes
	for(int el=0; el<mrRemesh.OutMesh.NumberOfElements; el++)
	  {
	    if( mrRemesh.PreservedElements[el]){
	      for(unsigned int pn=0; pn<nds; pn++)
		{
		  //set vertices
		  rNodes[OutElementList[el*nds+pn]].Set(ModelerUtilities::ENGAGED_NODES);
		}
	    }
	    
	  }

	int count_release = 0;
	for(ModelPart::NodesContainerType::iterator i_node = rNodes.begin() ; i_node != rNodes.end() ; i_node++)
	  {
	    if( i_node->IsNot(ModelerUtilities::ENGAGED_NODES)  ){
	      if(!(i_node->Is(FREE_SURFACE) || i_node->Is(RIGID))){
		i_node->Set(TO_ERASE);
		if( mEchoLevel > 0 )
		  std::cout<<" NODE "<<i_node->Id()<<" RELEASE "<<std::endl;
		if( i_node->IsNot(ModelerUtilities::ENGAGED_NODES) )
		  std::cout<<" ERROR: node "<<i_node->Id()<<" IS BOUNDARY RELEASE "<<std::endl;
		count_release++;
	      }
	    }
	      
	    i_node->Reset(ModelerUtilities::ENGAGED_NODES);
	  }
	  
	if( mEchoLevel > 0 )
	  std::cout<<"   NUMBER OF RELEASED NODES "<<count_release<<std::endl;

      }

      if( mEchoLevel > 0 ){
	std::cout<<"   Generated_Elements :"<<mrRemesh.OutMesh.NumberOfElements<<std::endl;
	std::cout<<"   Passed_AlphaShape  :"<<mrRemesh.Info->NumberOfElements<<std::endl;
	std::cout<<"   SELECT MESH ELEMENTS ]; "<<std::endl;
      }

      KRATOS_CATCH( "" )

    }


    /// this function is designed for being called at the beginning of the computations
    /// right after reading the model and the groups
    virtual void ExecuteInitialize()
    {
    }

    /// this function is designed for being execute once before the solution loop but after all of the
    /// solvers where built
    virtual void ExecuteBeforeSolutionLoop()
    {
    }

    /// this function will be executed at every time step BEFORE performing the solve phase
    virtual void ExecuteInitializeSolutionStep()
    {	
    }

    /// this function will be executed at every time step AFTER performing the solve phase
    virtual void ExecuteFinalizeSolutionStep()
    {
    }

    /// this function will be executed at every time step BEFORE  writing the output
    virtual void ExecuteBeforeOutputStep()
    {
    }

    /// this function will be executed at every time step AFTER writing the output
    virtual void ExecuteAfterOutputStep()
    {
    }

    /// this function is designed for being called at the end of the computations
    /// right after reading the model and the groups
    virtual void ExecuteFinalize()
    {
    }


    ///@}
    ///@name Access
    ///@{


    ///@}
    ///@name Inquiry
    ///@{


    ///@}
    ///@name Input and output
    ///@{

    /// Turn back information as a string.
    virtual std::string Info() const
    {
        return "SelectMeshElementsProcess";
    }

    /// Print information about this object.
    virtual void PrintInfo(std::ostream& rOStream) const
    {
        rOStream << "SelectMeshElementsProcess";
    }

    /// Print object's data.
    virtual void PrintData(std::ostream& rOStream) const
    {
    }


    ///@}
    ///@name Friends
    ///@{

    ///@}


private:
    ///@name Static Member Variables
    ///@{

    ///@}
    ///@name Static Member Variables
    ///@{
    ModelPart& mrModelPart;
 
    ModelerUtilities::MeshingParameters& mrRemesh;

    ModelerUtilities mModelerUtilities;  

    ModelPart::IndexType mMeshId; 

    int mEchoLevel;

    ///@}
    ///@name Un accessible methods
    ///@{

    ///@}



    /// Assignment operator.
    SelectMeshElementsProcess& operator=(SelectMeshElementsProcess const& rOther);


    /// this function is a private function


    /// Copy constructor.
    //Process(Process const& rOther);


    ///@}

}; // Class Process

///@}

///@name Type Definitions
///@{


///@}
///@name Input and output
///@{


/// input stream function
inline std::istream& operator >> (std::istream& rIStream,
                                  SelectMeshElementsProcess& rThis);

/// output stream function
inline std::ostream& operator << (std::ostream& rOStream,
                                  const SelectMeshElementsProcess& rThis)
{
    rThis.PrintInfo(rOStream);
    rOStream << std::endl;
    rThis.PrintData(rOStream);

    return rOStream;
}
///@}


}  // namespace Kratos.

#endif // KRATOS_SELECT_MESH_ELEMENTS_PROCESS_H_INCLUDED defined 
