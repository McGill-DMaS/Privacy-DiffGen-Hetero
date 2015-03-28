// TDPartitioner.cpp: implementation of the CTDPartitioner class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <map>

#if !defined(TDPARTITIONER_H)
    #include "TDPartitioner.h"
#endif


static int gPartitionIndex = 0;
static int gTestPartitionIndex = 0;



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTDPartitioner::CTDPartitioner(int nSpecialization, double pBudget) 
    : m_pAttribMgr(NULL),
	  m_pDataMgr(NULL), 
	  m_nSpecialization(nSpecialization), 
      m_pBudget(pBudget),
	  m_workingBudget(-1)
{
}

CTDPartitioner::~CTDPartitioner() 
{
    m_leafPartitions.cleanup();
	m_testLeafPartitions.cleanup();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDPartitioner::initialize(CTDAttribMgr* pAttribMgr, CTDDataMgr* pDataMgr)
{
    if (!pAttribMgr || !pDataMgr) {
        ASSERT(false);
        return false;
    }
    m_pAttribMgr = pAttribMgr;
    m_pDataMgr = pDataMgr;    
    return true;
}

//---------------------------------------------------------------------------
// The main algorithm.
//---------------------------------------------------------------------------
bool CTDPartitioner::transformData()
{
    cout << _T("Partitioning data...") << endl;

    // Initialize the first partition.
    CTDPartition* pRootPartition = initRootPartition();
    if (!pRootPartition)
        return false;

	// Initialize the generalized records of the first partition.
	if(!pRootPartition->initGenRecords(m_pAttribMgr->getAttributes())){
		delete pRootPartition;
		return false;
	}

	// We maintain a separate tree structure for test data to perform the same "cut" (genearalization)
	CTDPartition* pTestRootPartition = initTestRootPartition();
    if (!pTestRootPartition)
        return false;

	// initialize budget for exponential mechanism
	if(!initializeBudget()){
		ASSERT(false);
		return false;
	}

    // Register this root partition to the related concepts.
 	if (!pRootPartition->registerPartition()) {
        delete pRootPartition;
        return false;
    }

	// Register this root partition for test data
	if (!pTestRootPartition->testRegisterPartition()) {
        delete pTestRootPartition;
        return false;
    }
	
	// Adjust budget for determining the split point for all continuous attributes 
	m_pBudget = m_pBudget - (m_workingBudget * m_pAttribMgr->getNumConAttribs());

	// Construct raw counts of the partition.
    if (!pRootPartition->constructSupportMatrix(m_workingBudget)) {
        delete pRootPartition;
        return false;
    }
	
    // Compute score (e.g. infoGain or Max) of each concept in the cut.
    if (!m_pAttribMgr->computeScore()) {
        delete pRootPartition;
        return false;
    }

    // Add root partition to leaf partitions.
    m_leafPartitions.cleanup();
	pRootPartition->m_leafPos = m_leafPartitions.AddTail(pRootPartition);
    pRootPartition = NULL;


	// Add testRoot partition to testLeaf partitions.
	m_testLeafPartitions.cleanup();
	pTestRootPartition->m_leafPos = m_testLeafPartitions.AddTail(pTestRootPartition);
    pTestRootPartition = NULL;


    // Select an attribute to specialize.
    int splitCounter = 0;
    CTDAttrib* pSelectedAttrib = NULL;
    CTDConcept* pSelectedConcept = NULL;
	while (splitCounter < m_nSpecialization) {

		#ifdef _DEBUG_PRT_INFO
			cout << endl;
			cout << endl;
			cout << _T("* * * * * [Split Counter: ") << splitCounter << _T("] * * * * *") << endl;
		#endif
        
		// Adjust budget for picking winner attribube.
	    m_pBudget = m_pBudget - m_workingBudget;	
		
		// Select an concept for specialization
		if(!m_pAttribMgr->pickSpecializeConcept(pSelectedAttrib, pSelectedConcept, m_workingBudget)){
			m_leafPartitions.cleanup();
            return false;
		}
        
		// Adjust budget for determining the splitting point for continuous attribute if the winner is continuous attribute
		if(pSelectedAttrib->isContinuous()|| pSelectedAttrib->isSetValued()){
		    m_pBudget = m_pBudget - m_workingBudget;
		}
		
		// Split the related partitions based on the selected concept.
        if (!splitPartitions(pSelectedAttrib, pSelectedConcept)) {
            m_leafPartitions.cleanup();
            return false;
        }
	
		// Split the related partitions for test data
		if (!splitTestPartitions(pSelectedAttrib, pSelectedConcept)) {
            m_testLeafPartitions.cleanup();
            return false;
        }

        // Compute Score of each concept in the cut.
        if (!m_pAttribMgr->computeScore()) {
            m_leafPartitions.cleanup();
            return false;
        }

		++splitCounter;
    }
    
    cout << _T("Partitioning data succeeded.") << endl;
    
	return true;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

bool CTDPartitioner::addNoise()
{
	CTDPartition* pChildPartition = NULL;

	for (POSITION childPos = m_leafPartitions.GetHeadPosition(); childPos != NULL;) {
        pChildPartition = m_leafPartitions.GetNext(childPos);

        // Add noise to each leaf partition.
		if (!pChildPartition->addNoise(m_pBudget)) {
            ASSERT(false);
            return false;
        }
    }
	
	cout<< _T("The number of partitions are ")<< m_leafPartitions.GetSize()<< endl;
	cout << _T("Remaining privacy budget:")<< m_pBudget << endl;

	return true;
}


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartition* CTDPartitioner::initRootPartition()
{
    CTDPartition* pPartition = new CTDPartition(gPartitionIndex++, m_pAttribMgr->getAttributes());
    if (!pPartition)
        return NULL;

    CTDRecords* pRecs = m_pDataMgr->getRecords();
    if (!pRecs) {
        delete pPartition;
        return NULL;
    }

    int nRecs = pRecs->GetSize();
    for (int i = 0; i < nRecs; ++i) {
        if (!pPartition->addRecord(pRecs->GetAt(i))) {
            delete pPartition;
            return NULL;
        }
    }

    if (pPartition->getNumRecords() <= 0) {
        cerr << _T("CTDPartitioner: Zero number of records in root partition.") << endl;
        delete pPartition;
        ASSERT(false);
        return NULL;
    }
  
  return pPartition;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartition* CTDPartitioner::initTestRootPartition()
{
    CTDPartition* pPartition = new CTDPartition(gTestPartitionIndex++, m_pAttribMgr->getAttributes());
    if (!pPartition)
        return NULL;

    CTDRecords* pRecs = m_pDataMgr->getTestRecords();
    if (!pRecs) {
        delete pPartition;
        return NULL;
    }

    int nRecs = pRecs->GetSize();
    for (int i = 0; i < nRecs; ++i) {
        if (!pPartition->addRecord(pRecs->GetAt(i))) {
            delete pPartition;
            return NULL;
        }
    }

    if (pPartition->getNumRecords() <= 0) {
        cerr << _T("CTDPartitioner: Zero number of records in root test partition.") << endl;
        delete pPartition;
        ASSERT(false);
        return NULL;
    }
 
  return pPartition;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDPartitioner::initializeBudget()
{
	ASSERT(m_pBudget > 0 && m_nSpecialization > 0);
	m_workingBudget = m_pBudget/(2 * (m_pAttribMgr->getNumConAttribs() + 2 * m_nSpecialization));
	
	return true;
}	
//---------------------------------------------------------------------------
// For each related partitions of the split concept {

//      Distribute records from parent paritition to child partitions.
//      Register and compute statistics for the new partitions.
//---------------------------------------------------------------------------
bool CTDPartitioner::splitPartitions(CTDAttrib* pSplitAttrib, CTDConcept* pSplitConcept)
{
    ASSERT(pSplitAttrib && pSplitConcept);

    // Combine all the partitions
    CTDPartitions childPartitions, allChildPartitions;
	CTDPartitions allRelatedPartitions;
    CTDPartition* pParentPartition = NULL;
    CTDPartition* pChildPartition = NULL;
	CTDPartitions* pRelParts = NULL;
	if (pSplitAttrib->isSetValued()){
		CTDPartitions* pLinkedParts = NULL;
		CTDHierarchyCut* pLinkedHierarchyCut = NULL;
		CTDHierarchyCuts* pLinkedHierarchyCuts = pSplitConcept->getRelatedHierarchyCuts();
		allRelatedPartitions.RemoveAll();
		for (int i=0; i < pLinkedHierarchyCuts->GetSize(); ++i){
			pLinkedHierarchyCut = pLinkedHierarchyCuts->GetAt(i);
			pLinkedParts = pLinkedHierarchyCut->getRelatedPartitions();
			allRelatedPartitions.AddTail(pLinkedParts);
		}
		pRelParts = &allRelatedPartitions;
	}
	else {
		pRelParts = pSplitConcept->getRelatedPartitions();
	}
	// For each partition
	for (POSITION partPos = pRelParts->GetHeadPosition(); partPos != NULL;) {
		pParentPartition = pRelParts->GetNext(partPos);

#ifdef _DEBUG_PRT_INFO                        
        cout << _T("----------------------[Splitting Parent Partition]------------------------") << endl;
        cout << *pParentPartition;
#endif
        
		// Deregister this parent partition from the related concepts.
		if (!pParentPartition->deregisterPartition())
			return false;
			
        // Distribute records from parent paritition to child partitions.
        if (!distributeRecords(pParentPartition, pSplitAttrib, pSplitConcept, childPartitions))
            return false;

        for (POSITION childPos = childPartitions.GetHeadPosition(); childPos != NULL;) {
            pChildPartition = childPartitions.GetNext(childPos);

            // Register this child partition to the related concepts.
            if (!pChildPartition->registerPartition())
                return false;

            // Add child partitions to leaf partitions.
            pChildPartition->m_leafPos = m_leafPartitions.AddTail(pChildPartition);
            //cout << _T("# of leaf partitions: ") << m_leafPartitions.GetCount() << endl;

#ifdef _DEBUG_PRT_INFO
            cout << _T("------------------------[Splitted Child Partition]------------------------") << endl;
            cout << *pChildPartition;
#endif
		
		}

		// Remove parent partition from leaf partitions.
		m_leafPartitions.RemoveAt(pParentPartition->m_leafPos);
		delete pParentPartition;
		pParentPartition = NULL;

		// Keep track of all new child partitions.
        allChildPartitions.AddTail(&childPartitions);
    }

    // For each new child partition in this split, compute support matrix and gain ratios.
    for (POSITION childPos = allChildPartitions.GetHeadPosition(); childPos != NULL;) {
        pChildPartition = allChildPartitions.GetNext(childPos);

        // Construct raw counts of the child partition.
        if (!pChildPartition->constructSupportMatrix(m_workingBudget)) {
            ASSERT(false);
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------
// spliting records among test partitions like the previous function
//---------------------------------------------------------------------------
bool CTDPartitioner::splitTestPartitions(CTDAttrib* pSplitAttrib, CTDConcept* pSplitConcept)
{
    ASSERT(pSplitAttrib && pSplitConcept);

   
	 // Combine all the partitions
    CTDPartitions childPartitions, allChildPartitions;
	CTDPartitions allRelatedPartitions;
    CTDPartition* pParentPartition = NULL;
    CTDPartition* pChildPartition = NULL;
	CTDPartitions* pRelParts = NULL;
	if (pSplitAttrib->isSetValued()){
		CTDPartitions* pLinkedParts = NULL;
		CTDHierarchyCut* pLinkedHierarchyCut = NULL;
		CTDHierarchyCuts* pLinkedHierarchyCuts = pSplitConcept->getRelatedHierarchyCuts();
		for (int i=0; i < pLinkedHierarchyCuts->GetSize(); ++i){
			pLinkedHierarchyCut = pLinkedHierarchyCuts->GetAt(i);
			pLinkedParts = pLinkedHierarchyCut->getTestRelatedPartitions();
			allRelatedPartitions.AddTail(pLinkedParts);
		}
		pRelParts = &allRelatedPartitions;
	}
	else {
		pRelParts = pSplitConcept->getTestRelatedPartitions();
	}
	
	// For each partition
	for (POSITION partPos = pRelParts->GetHeadPosition(); partPos != NULL;) {
        pParentPartition = pRelParts->GetNext(partPos);

		#ifdef _DEBUG_PRT_INFO                        
        cout << _T("----------------------[Splitting TEST Parent Partition]------------------------") << endl;
        cout << *pParentPartition;
		#endif
        // Deregister this parent partition from the related concepts.
        if (!pParentPartition->testDeregisterPartition())
            return false;
        // Distribute records from parent paritition to child partitions.
        if (!testDistributeRecords(pParentPartition, pSplitAttrib, pSplitConcept, childPartitions))
            return false;

		for (POSITION childPos = childPartitions.GetHeadPosition(); childPos != NULL;) {
            pChildPartition = childPartitions.GetNext(childPos);

            // Register this child partition to the related concepts.
            if (!pChildPartition->testRegisterPartition())
                return false;

            // Add child partitions to leaf partitions.
            pChildPartition->m_leafPos = m_testLeafPartitions.AddTail(pChildPartition);
            //cout << _T("# of leaf partitions: ") << m_leafPartitions.GetCount() << endl;
			#ifdef _DEBUG_PRT_INFO
            cout << _T("------------------------[Splitted TEST Child Partition]------------------------") << endl;
            cout << *pChildPartition;
			#endif
        }

        // Remove parent partition from leaf partitions.
        m_testLeafPartitions.RemoveAt(pParentPartition->m_leafPos);
        delete pParentPartition;
        pParentPartition = NULL;
        
    }

    return true;
}


//---------------------------------------------------------------------------
// Distribute records from parent paritition to child partitions.
//---------------------------------------------------------------------------
bool CTDPartitioner::distributeRecords(CTDPartition*  pParentPartition,
                                       CTDAttrib*     pSplitAttrib, 
                                       CTDConcept*    pSplitConcept, 
                                       CTDPartitions& childPartitions) 
{
    childPartitions.RemoveAll();

	if (pSplitAttrib->isSetValued()){
	
		int index;		//This is the index of the clild hierarchycut
		CTDConcept* pChildConcept = NULL;
		CTDConcepts* pNewChildConcepts = NULL;  // new concept values for the child hierarchyCut
		CTDRecord* pRec = NULL;
		CTDValue* pSplitValue = NULL;
		CTDHierarchyCut* pParentHierarchyCut = NULL;
		int splitIdx = pSplitAttrib->m_attribIdx;
		pParentHierarchyCut = pParentPartition->getGenRecords()->GetAt(0)->getValue(splitIdx)->getCurrentHierarchyCut();
		
		ASSERT(pParentHierarchyCut);

		//it stores all non-empty subpartitions
		map<int, CTDPartition*>* spm = new map<int, CTDPartition*>;
		map<int, CTDPartition*>::iterator it;

		
	    // Scan through each record in the parent partition and
		// determine the child hierarchyCut. If it does not exist, then create one. 
		// Using the child Index, determine the partition and then assign the record.
		int nRecs = pParentPartition->getNumRecords();	
		for (int r = 0; r < nRecs; ++r) {       
			pRec = pParentPartition->getRecord(r);
			pSplitValue = pRec->getValue(splitIdx);
			
			pNewChildConcepts = new CTDConcepts();
			index = 0;
			for(int i=0; i < pSplitConcept->getNumChildConcepts(); i++){
				pChildConcept = pSplitConcept->getChildConcept(i);
				int in = pRec->hasConceptInRange(pChildConcept); //in equals either 1 or 0; 1 means there exist a concept
				index = index + (int) (pow(2.0, i)*in);
				if (in == 1)
					pNewChildConcepts->Add(pChildConcept);
			}	
			ASSERT(index >= 1); 

			CTDHierarchyCut* pChildHierarchyCut = NULL;
			pChildHierarchyCut = pParentHierarchyCut->getChildHierarchyCut(index);

			// If there is no HierarchyCut, then create one.
			if (pChildHierarchyCut == NULL)
			{
				pChildHierarchyCut = new CTDHierarchyCut(pSplitAttrib);
				pParentHierarchyCut->addChildConcept(pChildHierarchyCut, index);
				pChildHierarchyCut->setConceptsValues(pSplitConcept, pNewChildConcepts);
				pChildHierarchyCut->registerHierarchyCut();
				pChildHierarchyCut->m_flattenIdx = pSplitAttrib->getHierarchyCuts()->Add(pChildHierarchyCut);
			}

			delete pNewChildConcepts;
			pNewChildConcepts = NULL;

			ASSERT(pChildHierarchyCut);
			// lower the hierarchyCut
			if(!pSplitValue->setCurHierarchyCut(pChildHierarchyCut)){
				ASSERT(false);
				return false;
			}
		
			it = spm->find(index);

			//if this is a new 
			if (it == spm->end()){
				
				// Construct a partition for the HierarchyCut. 
				CTDPartition* pPartition = new CTDPartition(gPartitionIndex++, m_pAttribMgr->getAttributes());
				if (!pPartition){
					ASSERT(false);
					return false;
				}

				if (!pPartition->genRecords(pParentPartition, pSplitAttrib, pSplitConcept, m_pAttribMgr->getAttributes(), index)) {
						ASSERT(false);
						return false;
				}
				// Add the record to this child partition.
				if (!pPartition->addRecord(pRec)) {
					childPartitions.cleanup();
					ASSERT(false);                
					return false;
				}
				spm->insert(pair<int, CTDPartition*>(index, pPartition));
			}
			else
				it->second->addRecord(pRec);
		}

		
		//an array used to generate non-empty partitions
		int maxIndex = (int) pow(2.0,  pSplitConcept->getNumChildConcepts());
		
		for (int idx = 1; idx < maxIndex; idx++ ){
			int noisyCount = (int)laplaceNoise(m_workingBudget);	
			it = spm->find(idx);
			if (it == spm->end()){
				if(noisyCount > (sqrt(2.0)/m_workingBudget)){
					
					// Find hierarchy cut
					CTDHierarchyCut* pChildHierarchyCut = NULL;
					pChildHierarchyCut = pParentHierarchyCut->getChildHierarchyCut(idx);

					// If there is no HierarchyCut, then create one.
					if (pChildHierarchyCut == NULL)
					{
						pChildHierarchyCut = new CTDHierarchyCut(pSplitAttrib);
						pParentHierarchyCut->addChildConcept(pChildHierarchyCut, idx);
						CTDConcepts* pNewChildConcepts = NULL;  // new concept values for the child hierarchyCut
						pNewChildConcepts = pChildHierarchyCut->getChildValues(idx, pSplitConcept->getChildConcepts());  // new concept values for the child hierarchyCut
						pChildHierarchyCut->setConceptsValues(pSplitConcept, pNewChildConcepts);
						pChildHierarchyCut->registerHierarchyCut();
						pChildHierarchyCut->m_flattenIdx = pSplitAttrib->getHierarchyCuts()->Add(pChildHierarchyCut);
						
						delete pNewChildConcepts;
						pNewChildConcepts = NULL;
					}

					// Construct a partition for the HierarchyCut. 
					CTDPartition* pPartition = new CTDPartition(gPartitionIndex++, m_pAttribMgr->getAttributes());
					if (!pPartition){
						ASSERT(false);
						return false;
					}
					// create GenRecords
					if (!pPartition->genRecords(pParentPartition, pSplitAttrib, pSplitConcept, m_pAttribMgr->getAttributes(), idx)) {
						ASSERT(false);
						return false;
					}
					// Add to child partition
					childPartitions.AddTail(pPartition);

				}
			}
			else {
				CTDPartition* pPartition = it->second;
				//childPartitions.AddTail(pPartition);
				
				if((pPartition->getAllRecords()->GetSize() + noisyCount) > sqrt(2.0)/m_workingBudget)
					childPartitions.AddTail(pPartition);
				else{
					spm->erase(it);
					delete pPartition;
					pPartition = NULL;
				}
			}

		}

	}  // End of isSetValued()
	else {
		// Construct a partition for each child concept. 
		for (int childIdx = 0; childIdx < pSplitConcept->getNumChildConcepts(); ++childIdx)
			childPartitions.AddTail(new CTDPartition(gPartitionIndex++, m_pAttribMgr->getAttributes()));

		CTDPartition* pChildPartition = NULL;
		int idx = 0;
	
		for (POSITION childPos = childPartitions.GetHeadPosition(); childPos != NULL; ++idx) {
			pChildPartition = childPartitions.GetNext(childPos);

			if (!pChildPartition->genRecords(pParentPartition, pSplitAttrib, pSplitConcept, m_pAttribMgr->getAttributes(), idx)) {
				ASSERT(false);
				return false;
			}
		}

	    // Scan through each record in the parent partition and
		// add records to the corresponding child partition based
		// on the child concept.
		CTDRecord* pRec = NULL;
		CTDValue* pSplitValue = NULL;
		POSITION childPartitionPos = NULL;
		int childConceptIdx = -1;
		int splitIdx = pSplitAttrib->m_attribIdx;
		int nRecs = pParentPartition->getNumRecords();
		//   ASSERT(nRecs > 0);
		for (int r = 0; r < nRecs; ++r) {
			pRec = pParentPartition->getRecord(r);
			pSplitValue = pRec->getValue(splitIdx);

			// Lower the concept by one level.
			if (!pSplitValue->lowerCurrentConcept()) {
				cerr << _T("CTDPartition: Should not specialize on this concept.");
				childPartitions.cleanup();
				ASSERT(false);
				return false;
			}

			// Get the child concept of the current concept in this record.
			childConceptIdx = pSplitValue->getCurrentConcept()->m_childIdx;
			ASSERT(childConceptIdx != -1);
			childPartitionPos = childPartitions.FindIndex(childConceptIdx);
			ASSERT(childPartitionPos);

			// Add the record to this child partition.
			if (!childPartitions.GetAt(childPartitionPos)->addRecord(pRec)) {
				childPartitions.cleanup();
				ASSERT(false);                
				return false;
			}
		}

		// Delete empty child partitions.
		//childPartitions.deleteEmptyPartitions();
		
	}
	
    return true;
}
//---------------------------------------------------------------------------
// Distribute records from parent paritition to child partitions.
//---------------------------------------------------------------------------
bool CTDPartitioner::testDistributeRecords(CTDPartition*  pParentPartition,
                                       CTDAttrib*     pSplitAttrib, 
                                       CTDConcept*    pSplitConcept, 
                                       CTDPartitions& childPartitions) 
{
    childPartitions.RemoveAll();
	
	if (pSplitAttrib->isSetValued()){
	
		int index;		//This is the index of the clild hierarchycut
		CTDConcept* pChildConcept = NULL;  
		CTDConcepts* pNewChildConcepts = NULL;  // new concept values for the child hierarchyCut
		CTDRecord* pRec = NULL;
		CTDValue* pSplitValue = NULL;
		CTDHierarchyCut* pParentHierarchyCut = NULL;
		int splitIdx = pSplitAttrib->m_attribIdx;
		pParentHierarchyCut = pParentPartition->getRecord(0)->getValue(splitIdx)->getCurrentHierarchyCut();

		ASSERT(pParentHierarchyCut);

		//it stores all non-empty subpartitions
		map<int, CTDPartition*>* spm = new map<int, CTDPartition*>;
		map<int, CTDPartition*>::iterator it;

		
	    // Scan through each record in the parent partition and
		// determine the child hierarchyCut. If it does not exist, then create one. 
		// Using the child Index, determine the partition and then assign the record.
		int nRecs = pParentPartition->getNumRecords();	
		for (int r = 0; r < nRecs; ++r) {       
			pRec = pParentPartition->getRecord(r);
			pSplitValue = pRec->getValue(splitIdx);
			CTDConcepts* pNewChildConcepts = NULL;  // new concept values for the child hierarchyCut

			pNewChildConcepts = new CTDConcepts();
			index = 0;
			for(int i=0; i < pSplitConcept->getNumChildConcepts(); i++){
				pChildConcept = pSplitConcept->getChildConcept(i);
				int in = pRec->hasConceptInRange(pChildConcept); //in equals either 1 or 0; 1 means there exist a concept
				index = index + (int) (pow(2.0, i)*in);
				if (in == 1)
					pNewChildConcepts->Add(pChildConcept);
			}	
			
			if (index == 0){
				cerr << _T("CTDPartitioner::testDistributeRecords: wrong input value:") << pSplitValue->toString(true);
				ASSERT(false); 
				return false;
			}

			CTDHierarchyCut* pChildHierarchyCut = NULL;
			pChildHierarchyCut = pParentHierarchyCut->getChildHierarchyCut(index);

			// If there is no HierarchyCut, then create one.
			if (pChildHierarchyCut == NULL)
			{
				pChildHierarchyCut = new CTDHierarchyCut(pSplitAttrib);
				pParentHierarchyCut->addChildConcept(pChildHierarchyCut, index);
				pChildHierarchyCut->setConceptsValues(pSplitConcept, pNewChildConcepts);
				pChildHierarchyCut->registerHierarchyCut();
				pChildHierarchyCut->m_flattenIdx = pSplitAttrib->getHierarchyCuts()->Add(pChildHierarchyCut);
			}

			delete pNewChildConcepts;
			pNewChildConcepts = NULL;

			ASSERT(pChildHierarchyCut);

			// lower the hierarchyCut
			if(!pSplitValue->setCurHierarchyCut(pChildHierarchyCut)){
				ASSERT(false);
				return false;
			}
		
			it = spm->find(index);

			//if this is a new 
			if (it == spm->end()){
				
				// Construct a partition for the HierarchyCut. 
				CTDPartition* pPartition = new CTDPartition(gTestPartitionIndex++, m_pAttribMgr->getAttributes());
				if (!pPartition){
					ASSERT(false);
					return false;
				}
				
				childPartitions.AddTail(pPartition);

				// Add the record to this child partition.
				if (!pPartition->addRecord(pRec)) {
					childPartitions.cleanup();
					ASSERT(false);                
					return false;
				}
				spm->insert(pair<int, CTDPartition*>(index, pPartition));

			}
			else
				it->second->addRecord(pRec);
		}

	}  // End of isSetValued()
	else {

		// Construct a partition for each child concept. 
		for (int childIdx = 0; childIdx < pSplitConcept->getNumChildConcepts(); ++childIdx)
			childPartitions.AddTail(new CTDPartition( gTestPartitionIndex++, m_pAttribMgr->getAttributes()));

		// Scan through each record in the parent partition and
		// add records to the corresponding child partition based
		// on the child concept.
		CTDRecord* pRec = NULL;
		CTDValue* pSplitValue = NULL;
		POSITION childPartitionPos = NULL;
		int childConceptIdx = -1;
		int splitIdx = pSplitAttrib->m_attribIdx;
		int nRecs = pParentPartition->getNumRecords();
		ASSERT(nRecs > 0);
		for (int r = 0; r < nRecs; ++r) {
			pRec = pParentPartition->getRecord(r);
			pSplitValue = pRec->getValue(splitIdx);

			// Lower the concept by one level.
			if (!pSplitValue->lowerCurrentConcept()) {
				cerr << _T("CTDPartition: Should not specialize on this concept.");
				childPartitions.cleanup();
				ASSERT(false);
				return false;
			}

			// Get the child concept of the current concept in this record.
			childConceptIdx = pSplitValue->getCurrentConcept()->m_childIdx;
			ASSERT(childConceptIdx != -1);
			childPartitionPos = childPartitions.FindIndex(childConceptIdx);
			ASSERT(childPartitionPos);

			// Add the record to this child partition.
			if (!childPartitions.GetAt(childPartitionPos)->addRecord(pRec)) {
				childPartitions.cleanup();
				ASSERT(false);                
				return false;
			}
		}
		// Delete empty child partitions.
		childPartitions.deleteEmptyPartitions();

	}
  
    return true;
}
