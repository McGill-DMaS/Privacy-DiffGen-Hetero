// TDConcept.cpp: implementation of the CTDConcept class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if !defined(TDCONCEPT_H)
    #include "TDConcept.h"
#endif

#if !defined(TDPARTITION_H)
    #include "TDPartition.h"
#endif

//**************
// CTDConcepts *
//**************
CTDConcepts::CTDConcepts()
{
}

CTDConcepts::~CTDConcepts()
{
}

void CTDConcepts::cleanup()
{
    for (int i = 0; i < GetSize(); ++i)
        delete GetAt(i);

    RemoveAll();
}

CString CTDConcepts::toString()
{
	CString str;
	str += GetAt(0)->toString();
	for (int i = 1; i < GetSize(); ++i){
		str += _T("-");
		str += GetAt(i)->toString();
	}		
    return str;
}

//*************
// CTDConcept *
//*************

CTDConcept::CTDConcept(CTDAttrib* pAttrib)
    : m_pParentConcept(NULL), 
      m_pAttrib(pAttrib),
      m_childIdx(-1), 
      m_flattenIdx(-1), 
      m_depth(-1),
      m_infoGain(-1.0f), 
	  m_max(-1.0f),
      m_bCutCandidate(true),
      m_cutPos(NULL),
      m_pSplitSupMatrix(NULL),
	  m_minimum(-1),
	  m_maximum(-1)
{
    // use dynamic alocation in order to use forward declaration.
    m_pRelatedPartitions = new CTDPartitions();
	m_pTestRelatedPartitions = new CTDPartitions();
	m_pRelatedHierchyCuts = new CTDHierarchyCuts(); 
}

CTDConcept::~CTDConcept() 
{
    delete m_pSplitSupMatrix;
    m_pSplitSupMatrix = NULL;

    delete m_pRelatedPartitions;
    m_pRelatedPartitions = NULL;

	delete m_pTestRelatedPartitions;
	m_pTestRelatedPartitions = NULL;

	delete m_pRelatedHierchyCuts;
	m_pRelatedHierchyCuts = NULL;

	m_childConcepts.cleanup();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDConcept::addChildConcept(CTDConcept* pConceptNode)
{
    try {
        pConceptNode->m_pParentConcept = this;
        pConceptNode->m_childIdx = m_childConcepts.Add(pConceptNode);
        return true;
    }
    catch (CMemoryException&) {
        ASSERT(false);
        return false;
    }
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int CTDConcept::getNumChildConcepts() const
{ 
    return m_childConcepts.GetSize(); 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDConcept* CTDConcept::getChildConcept(int idx) const
{
    return m_childConcepts.GetAt(idx); 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDConcept* CTDConcept::getParentConcept() 
{ 
    return m_pParentConcept; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartitions* CTDConcept::getRelatedPartitions() 
{ 
    return m_pRelatedPartitions; 
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartitions* CTDConcept::getTestRelatedPartitions() 
{ 
    return m_pTestRelatedPartitions; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
POSITION CTDConcept::registerPartition(CTDPartition* pPartition) 
{ 
    return m_pRelatedPartitions->AddTail(pPartition);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
POSITION CTDConcept::testRegisterPartition(CTDPartition* pPartition) 
{ 
    return m_pTestRelatedPartitions->AddTail(pPartition);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CTDConcept::deregisterPartition(POSITION pos) 
{
    m_pRelatedPartitions->RemoveAt(pos);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CTDConcept::testDeregisterPartition(POSITION pos) 
{
    m_pTestRelatedPartitions->RemoveAt(pos);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDHierarchyCuts* CTDConcept::getRelatedHierarchyCuts() 
{ 
    return m_pRelatedHierchyCuts; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int CTDConcept::registerHierarchyCut(CTDHierarchyCut* pHierarchyCut)
{
	return m_pRelatedHierchyCuts->Add(pHierarchyCut);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CTDConcept::deregisterHierarchyCut(int idx) 
{
	m_pRelatedHierchyCuts->RemoveAt(idx);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDConcept::initSplitMatrix(int nConcepts, int nClasses)
{
    // Allocate the matrix
    int dims[] = {nConcepts, nClasses};
    m_pSplitSupMatrix = new CTDMDIntArray(sizeof(dims) / sizeof(int), dims);
    if (!m_pSplitSupMatrix) {
        ASSERT(false);
        return false;
    }

    // Allocate support sums
    m_splitSupSums.SetSize(nConcepts);

    // Allocate class sums
    m_splitClassSums.SetSize(nClasses);

    for (int i = 0; i < nConcepts; ++i) {
        for (int j = 0; j < nClasses; ++j) {
            (*m_pSplitSupMatrix)[i][j] = 0;
            m_splitClassSums.SetAt(j, 0);
        }
        m_splitSupSums.SetAt(i, 0);
    }
    return true;
}

//---------------------------------------------------------------------------
// Compute Score from all linked partitions.
//---------------------------------------------------------------------------
bool CTDConcept::computeScore(int nClasses, int classIdx)
{
    if (m_infoGain != -1.0f || !m_bCutCandidate)
        return true;

    if (isSetValued()){
		
		// Combine all the records.
		CTDRecords jointRecs;
		jointRecs.RemoveAll();
		CTDHierarchyCut* pLinkedHierarchyCut = NULL;
		CTDPartitions* pLinkedParts = NULL;

		CTDHierarchyCuts* pLinkedHierarchyCuts = getRelatedHierarchyCuts();
		
		for (int i=0; i < pLinkedHierarchyCuts->GetSize(); ++i){
			
			pLinkedHierarchyCut = pLinkedHierarchyCuts->GetAt(i);
			pLinkedParts = pLinkedHierarchyCut->getRelatedPartitions();
		
			for (POSITION pos = pLinkedParts->GetHeadPosition(); pos != NULL;) 
				jointRecs.Append(*(pLinkedParts->GetNext(pos)->getAllRecords()));

		}

		// Allocate the matrix
		//number of possible hierarchycuts
		int nChildHierarchycuts = (int) pow(2.0, getNumChildConcepts());
		int dims[] = {nChildHierarchycuts, nClasses};
		CTDMDIntArray supMatrix(sizeof(dims) / sizeof(int), dims);

		// Allocate support sums
		CTDIntArray supSums;
		supSums.SetSize(nChildHierarchycuts);

		// Allocate class sums
		CTDIntArray classSums;
		classSums.SetSize(nClasses);
    
		// Initialization
		int i = 0;
		int j = 0;
		for (i = 0; i < nChildHierarchycuts; ++i) {
			for (j = 0; j < nClasses; ++j) {
				supMatrix[i][j] = 0;
				classSums.SetAt(j, 0);
			}
			supSums.SetAt(i, 0);
		}

		int index;		//index is used to determine the clild hierarchycut
		CTDConcept* pClassConcept = NULL;
		CTDConcept* pChildConcept = NULL;
		CTDRecord* pRec = NULL;
		int nRecs = jointRecs.GetSize();
		for (int r = 0; r < nRecs; ++r) {       
			
			pRec = jointRecs.GetAt(r);
			pClassConcept = pRec->getValue(classIdx)->getCurrentConcept();
			
			++classSums[pClassConcept->m_childIdx];

			index = -1;

			for(int i=0; i < getNumChildConcepts(); i++){
				pChildConcept = getChildConcept(i);
				int in = pRec->hasConceptInRange(pChildConcept); //in equals either 1 or 0
				index = index + (int) (pow(2.0, i)*in);
				
			}	

			ASSERT(index >= 0 || index < nChildHierarchycuts); 

			++supSums[index];
			++supMatrix[index][pClassConcept->m_childIdx];
		}

		if (!computeMaxHelper(supSums, classSums, supMatrix, m_max)) {
			ASSERT(false);
			return false;
		}
		if (!computeInfoGainHelper(computeEntropy(&classSums), supSums, classSums, supMatrix, m_infoGain)) {
			ASSERT(false);
			return false;
		}
	}
	else
	{
		// Allocate the matrix
		int nChildConcepts = getNumChildConcepts();
		int dims[] = {nChildConcepts, nClasses};
		CTDMDIntArray supMatrix(sizeof(dims) / sizeof(int), dims);

		// Allocate support sums
		CTDIntArray supSums;
		supSums.SetSize(nChildConcepts);

		// Allocate class sums
		CTDIntArray classSums;
		classSums.SetSize(nClasses);
    
		// Initialization
		int i = 0;
		int j = 0;
		for (i = 0; i < nChildConcepts; ++i) {
			for (j = 0; j < nClasses; ++j) {
				supMatrix[i][j] = 0;
				classSums.SetAt(j, 0);
			}
			supSums.SetAt(i, 0);
		}

		// For each linked partition.
		CTDPartAttrib* pPartAttrib = NULL;
		CTDPartition* pLinkedPart = NULL;
		CTDPartitions* pLinkedParts = getRelatedPartitions();
		for (POSITION partPos = pLinkedParts->GetHeadPosition(); partPos != NULL;) {
			pLinkedPart = pLinkedParts->GetNext(partPos);
			pPartAttrib = pLinkedPart->getPartAttribs()->GetAt(pLinkedPart->getPartAttribs()->FindIndex(m_pAttrib->m_attribIdx));
			if (!pPartAttrib->m_bCandidate) {
				m_bCutCandidate = false;
				return true;
			} 
        
			// Add up all the counts.
			ASSERT(pPartAttrib->getSupportSums()->GetSize() == nChildConcepts);        
			for (i = 0; i < nChildConcepts; ++i) {
				for (j = 0; j < nClasses; ++j) {
					supMatrix[i][j] += (*pPartAttrib->getSupportMatrix())[i][j];
				}
				supSums[i] += (*pPartAttrib->getSupportSums())[i];
			}

			ASSERT(pPartAttrib->getClassSums()->GetSize() == nClasses);
			for (j = 0; j < nClasses; ++j) {
				classSums[j] += (*pPartAttrib->getClassSums())[j];
			}
		}

		if (!computeMaxHelper(supSums, classSums, supMatrix, m_max)) {
			ASSERT(false);
			return false;
		}
		if (!computeInfoGainHelper(computeEntropy(&classSums), supSums, classSums, supMatrix, m_infoGain)) {
			ASSERT(false);
			return false;
		}
	
	}

    return true;
}

//---------------------------------------------------------------------------
// Compute Max count from linked partitions.
//---------------------------------------------------------------------------
// static
bool CTDConcept::computeMaxHelper(const CTDIntArray& supSums, 
                                  const CTDIntArray& classSums,
								  CTDMDIntArray& supMatrix,
                                  float& max)
{
    max = 0.0f;
	int cMax = 0, totalMax = 0;
    int s = 0, c = 0;
    
	int nSupports = supSums.GetSize();    
	int nClasses = classSums.GetSize();

	for (s = 0; s < nSupports; ++s) {
        cMax = 0;
		for (c = 0; c < nClasses; ++c) {
    		if ( supMatrix[s][c] > cMax) {
                cMax = supMatrix[s][c];
            }
		} 
		totalMax = totalMax + cMax;
   	}
    
	max = (float) totalMax;

    return true;
}


//---------------------------------------------------------------------------
// Compute information gain from linked partitions.
//---------------------------------------------------------------------------
// static
bool CTDConcept::computeInfoGainHelper(float entropy, 
                                       const CTDIntArray& supSums, 
                                       const CTDIntArray& classSums, 
                                       CTDMDIntArray& supMatrix,
                                       float& infoGainDiff)
{
    infoGainDiff = 0.0f;
    int total = 0, s = 0;
    int nSupports = supSums.GetSize();    
    for (s = 0; s < nSupports; ++s)
        total += supSums.GetAt(s);
    
	if (total == 0)
		return true;

//	ASSERT(total > 0);

    int nClasses = classSums.GetSize();
    int c = 0;
    float r = 0.0f, mutualInfo = 0.0f, infoGainS = 0.0f;
    for (s = 0; s < nSupports; ++s) {
        infoGainS = 0.0f;
        for (c = 0; c < nClasses; ++c) {
            //ASSERT(supSums->GetAt(s) > 0); It is possible that some classes have 0 support.
            r = float(supMatrix[s][c]) / supSums.GetAt(s);
            if (r > 0.0f) 
                infoGainS += (r * log2f(r)) * -1; 
        }        
        mutualInfo += (float(supSums.GetAt(s)) / total) * infoGainS;
    }
    infoGainDiff = entropy - mutualInfo;
    return true;
}

//---------------------------------------------------------------------------
// Compute entropy of all linked partitions.
//---------------------------------------------------------------------------
// static
float CTDConcept::computeEntropy(CTDIntArray* pClassSums) 
{
    float entropy = calEntropy(pClassSums);
/*  
	if (entropy == 0.0f) {
        // One class in this partition.
        return FLT_MAX;
    }
*/
    return entropy;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDConcept::parseFirstConcept(CString& firstConcept, CString& restStr)
{
    firstConcept.Empty();
    int len = restStr.GetLength();
    if (len < 2 ||
        restStr[0] !=  TD_CONHCHY_OPENTAG || 
        restStr[len - 1] !=  TD_CONHCHY_CLOSETAG) {
		ASSERT(false);
        return false;
    }

    // Find the index number of the closing tag of the first concept.
    int tagCount = 0;
    for (int i = 0; i < len; ++i) {
        if (restStr[i] == TD_CONHCHY_OPENTAG)
            ++tagCount;
        else if (restStr[i] == TD_CONHCHY_CLOSETAG) {
            --tagCount;
            ASSERT(tagCount >= 0);
            if (tagCount == 0) {
                // Closing tag of first concept found!
                firstConcept = restStr.Left(i + 1);
                restStr = restStr.Mid(i + 1);
                CBFStrHelper::trim(restStr);
                return true;
            }
        }
    }
    ASSERT(false);
    return false;
}

//*****************
// CTDDiscConcept *
//*****************

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDDiscConcept::CTDDiscConcept(CTDAttrib* pAttrib) 
    : CTDConcept(pAttrib), m_pSplitConcept(NULL)
{
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDDiscConcept::~CTDDiscConcept() 
{
}

//---------------------------------------------------------------------------
// {Any Location {BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}}
//---------------------------------------------------------------------------
bool CTDDiscConcept::initHierarchy(LPCTSTR conceptStr, int depth, CTDIntArray& maxBranches)
{
    // Parse the conceptValue and the rest of the string.
    CString restStr;
    if (!parseConceptValue(conceptStr, m_conceptValue, restStr)) {
        cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << conceptStr << endl;
        return false;
    }
    m_depth = depth;

    // Depth-first build.
    CString firstConcept;
    while (!restStr.IsEmpty()) {
        if (!parseFirstConcept(firstConcept, restStr)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << restStr << endl;
            return false;
        }

        CTDDiscConcept* pNewConcept = new CTDDiscConcept(m_pAttrib);
        if (!pNewConcept)
            return false;
        
        if (!pNewConcept->initHierarchy(firstConcept, depth + 1, maxBranches)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << firstConcept << endl;
            return false;
        }

        if (!addChildConcept(pNewConcept))
            return false;
    }

    // Update the maximum # of branches at this level
    int nChildren = getNumChildConcepts();
    if (nChildren > 0) {
        while (depth > maxBranches.GetUpperBound())
            maxBranches.Add(0);
        if (nChildren > maxBranches[depth])
            maxBranches[depth] = nChildren;
    }
    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CString CTDDiscConcept::toString()
{
    return m_conceptValue;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDDiscConcept::parseConceptValue(LPCTSTR str, CString& conceptVal, CString& restStr)
{
    conceptVal.Empty();
    restStr.Empty();

    CString wrkStr = str;
    if (wrkStr.GetLength() < 2 ||
        wrkStr[0] !=  TD_CONHCHY_OPENTAG || 
        wrkStr[wrkStr.GetLength() - 1] !=  TD_CONHCHY_CLOSETAG) {
        ASSERT(false);
        return false;
    }

    // Extract "Canada {BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}"
    wrkStr = wrkStr.Mid(1, wrkStr.GetLength() - 2);
    CBFStrHelper::trim(wrkStr);
    if (wrkStr.IsEmpty()) {
        ASSERT(false);
        return false;
    }

    // Extract "Canada"
    int openPos = wrkStr.Find(TD_CONHCHY_OPENTAG);
    if (openPos < 0) {
        // This is root value, e.g., "Vancouver".
        conceptVal = wrkStr;
        return true;
    }
    else {
        conceptVal = wrkStr.Left(openPos);
        CBFStrHelper::trim(conceptVal);
        if (conceptVal.IsEmpty()) {
            ASSERT(false);
            return false;
        }
    }

    // Extract "{BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}"
    restStr = wrkStr.Mid(openPos);
    return true;
}

//*****************
// CTDContConcept *
//*****************

CTDContConcept::CTDContConcept(CTDAttrib* pAttrib) 
    : CTDConcept(pAttrib), m_lowerBound(0.0f), m_upperBound(0.0f), m_splitPoint(FLT_MAX)
{
}

CTDContConcept::~CTDContConcept() 
{
}

//---------------------------------------------------------------------------
// {0-100 {0-50 {<25} {25-50}} {50-100 {50-75} {75-100}}}
//---------------------------------------------------------------------------
bool CTDContConcept::initHierarchy(LPCTSTR conceptStr, int depth, CTDIntArray& maxBranches)
{
    // Parse the conceptValue and the rest of the string.
    CString restStr;
    if (!parseConceptValue(conceptStr, m_conceptValue, restStr, m_lowerBound, m_upperBound)) {
        cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << conceptStr << endl;
        return false;
    }

#ifdef _TD_MANUAL_CONTHRCHY

	m_depth = depth;

    // Depth-first build.
    CString firstConcept;
    while (!restStr.IsEmpty()) {
        if (!parseFirstConcept(firstConcept, restStr)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << restStr << endl;
            return false;
        }
		
		
        CTDContConcept* pNewConcept = new CTDContConcept(m_pAttrib);
        if (!pNewConcept)
            return false;
        
		
		if (!pNewConcept->initHierarchy(firstConcept, depth + 1, maxBranches)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << firstConcept << endl;
            return false;
        }

        if (!addChildConcept(pNewConcept))
            return false;
    }
#endif

    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CString CTDContConcept::toString()
{
#ifdef _TD_TREAT_CONT_AS_CONT
    CString str;
    LPTSTR tempStr = FloatToStr((m_lowerBound + m_upperBound) / 2.0f, TD_CONTVALUE_NUMDEC);
    str = tempStr;
    delete [] tempStr;
    return str;    
#else
    CString str;
    LPTSTR tempStr = FloatToStr(m_lowerBound, TD_CONTVALUE_NUMDEC);
    str += tempStr;
    str += _T("-");
    delete [] tempStr;
    tempStr = FloatToStr(m_upperBound, TD_CONTVALUE_NUMDEC);
    str += tempStr;
    delete [] tempStr;
    tempStr = NULL;
    return str;
#endif
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDContConcept::parseConceptValue(LPCTSTR str, 
                                       CString& conceptVal, 
                                       CString& restStr, 
                                       float& lowerBound, 
                                       float& upperBound)
{
    conceptVal.Empty();
    restStr.Empty();
    lowerBound = 0.0f;
    upperBound = 0.0f;

    CString wrkStr = str;
    if (wrkStr.GetLength() < 2 ||
        wrkStr[0] !=  TD_CONHCHY_OPENTAG || 
        wrkStr[wrkStr.GetLength() - 1] !=  TD_CONHCHY_CLOSETAG) {
        ASSERT(false);
        return false;
    }

    wrkStr = wrkStr.Mid(1, wrkStr.GetLength() - 2);
    CBFStrHelper::trim(wrkStr);
    if (wrkStr.IsEmpty()) {
        ASSERT(false);
        return false;
    }

    // Extract "0-100"
    LPTSTR tempStr = NULL;
    int openPos = wrkStr.Find(TD_CONHCHY_OPENTAG);
    if (openPos < 0) {
        // This is root value, e.g., "0-50".
        conceptVal = wrkStr;
        if (!parseLowerUpperBound(conceptVal, lowerBound, upperBound))
            return false;
    }
    else {
        conceptVal = wrkStr.Left(openPos);
        CBFStrHelper::trim(conceptVal);
        if (conceptVal.IsEmpty()) {
            ASSERT(false);
            return false;
        }

        if (!parseLowerUpperBound(conceptVal, lowerBound, upperBound))
            return false;
    }

    // Convert again to make sure exact match for decimal places.
    if (!makeRange(lowerBound, upperBound, conceptVal)) {
        ASSERT(false);
        return false;
    }

    restStr = wrkStr.Mid(openPos);
    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDContConcept::makeRange(float lowerB, float upperB, CString& range)
{
    LPTSTR tempStr = FloatToStr(lowerB, TD_CONTVALUE_NUMDEC);
    range = tempStr;
    delete [] tempStr;
    tempStr = NULL;
    tempStr = FloatToStr(upperB, TD_CONTVALUE_NUMDEC);
    range += _T("-");
    range += tempStr;
    delete [] tempStr;
    tempStr = NULL;
    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDContConcept::parseLowerUpperBound(const CString& str, float& lowerB, float& upperB)
{
    lowerB = upperB = 0.0f;

    int dashPos = str.Find(TD_CONHCHY_DASHSYM);
    if (dashPos < 0) {
        cerr << _T("CTDDiscConcept: Failed to parse ") << str << endl;
        ASSERT(false);
        return false;
    }

    CString lowStr = str.Left(dashPos);
    CBFStrHelper::trim(lowStr);
    if (lowStr.IsEmpty()) {
        cerr << _T("CTDDiscConcept: Failed to parse ") << str << endl;
        ASSERT(false);
        return false;
    }
    
    CString upStr = str.Mid(dashPos + 1);
    CBFStrHelper::trim(upStr);
    if (upStr.IsEmpty()) {
        cerr << _T("CTDDiscConcept: Failed to parse ") << str << endl;
        ASSERT(false);
        return false;
    }

    lowerB = (float) StrToFloat(lowStr);
    upperB = (float) StrToFloat(upStr);
    if (lowerB > upperB) {
        ASSERT(false);
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------
// Split this continuous concept.
// 1) Join all the records in related partitions.
// 2) Sort the joint recrods according to their raw values of this attribute.
// 3) Find the optimal split point.
// 4) Add the child concepts to this concept.
//---------------------------------------------------------------------------
bool CTDContConcept::divideConcept(double epsilon, int nClasses)
{
   
	#ifdef _TD_MANUAL_CONTHRCHY
		
		return true;

	#endif
	
	if (m_splitPoint != FLT_MAX) {
        // No need to find again.        
        return true;
    }

    // Join all the records in related partitions.
    CTDRecords jointRecs;
    if (!getRelatedPartitions()->joinPartitions(jointRecs))
        return false;

    if (jointRecs.GetSize() <= 1) {    
        // srand( (unsigned)time( NULL ) );
		//m_splitPoint = (float) (rand() % (int)(m_upperBound - m_lowerBound + 1) + m_lowerBound);
		m_splitPoint = (m_upperBound + m_lowerBound )/ 2;
	
	}
	else {
		 // Sort the joint recrods according to their raw values of this attribute.
         if (!jointRecs.sortByAttrib(m_pAttrib->m_attribIdx))
             return false;

         // Find optimal split point.
         if (!findOptimalSplitPoint(jointRecs, nClasses, epsilon))
             return false;
	}

    // Cannot split on this concept.
    if (!m_bCutCandidate)
		return true;
	
	// Add the child concepts to this concept.
    if (!makeChildConcepts())
		return false;
	
    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDContConcept::findOptimalSplitPoint(CTDRecords& recs, int nClasses, double epsilon)
{
    if (!initSplitMatrix(2, nClasses))
        return false;

    if (!m_pAttrib->isContinuous()) {
        ASSERT(false);
        return false;
    }

    int attribIdx = m_pAttrib->m_attribIdx;
    int classIdx = recs.GetAt(0)->getNumValues() - 1;
    int nRecs = recs.GetSize();

    // Initialize counters
	int r = 0;
    CTDRecord* pCurrRec = NULL;
    CTDRecord* pNextRec = NULL;
    CTDConcept* pClassConcept = NULL;
    for (r = 0; r < nRecs; ++r) {
        pCurrRec = recs.GetAt(r);
        pClassConcept = pCurrRec->getValue(classIdx)->getCurrentConcept();
        ++((*m_pSplitSupMatrix)[1][pClassConcept->m_childIdx]);
        ++(m_splitSupSums[1]);
        ++(m_splitClassSums[pClassConcept->m_childIdx]);
    }
    
	CTDRanges cRanges; 
	cRanges.cleanup();
	CTDFloatArray weights;
	CTDFloatArray ranges;

	// add one more element for the first range

	int idx = 0;

	float entropy = 0.0f;
    float infoGain = 0.0f;
	float max = 0.0f;
    bool FLAG = false;
    
	CTDNumericValue* pCurrValue = NULL;
    CTDNumericValue* pNextValue = NULL;
    for (r = 0; r < nRecs - 1; ++r) {
        pCurrRec = recs.GetAt(r);
        pNextRec = recs.GetAt(r + 1);
        pCurrValue = static_cast<CTDNumericValue*> (pCurrRec->getValue(attribIdx));
        pNextValue = static_cast<CTDNumericValue*> (pNextRec->getValue(attribIdx));
        if (!pCurrValue || !pNextValue) {
            ASSERT(false);
            return false;
		}

        // Create the fist range if the first value is not equal to the lowest possible value of the concept.
		// m_lowerbound is inclusive and m_upperbound is exclusive for a range. 
		if (r == 0 && pCurrValue->getRawValue()> m_lowerBound){
			cRanges.Add(new CTDRange(pCurrValue->getRawValue(), m_lowerBound));  
			ranges.Add(pCurrValue->getRawValue() - m_lowerBound);
			weights.Add(0.0f);
		}
		
		// Get the class concept.
        pClassConcept = pCurrRec->getValue(classIdx)->getCurrentConcept(); 

        // Compute support counters.            
        ++((*m_pSplitSupMatrix)[0][pClassConcept->m_childIdx]);
        --((*m_pSplitSupMatrix)[1][pClassConcept->m_childIdx]);
        
        // Compute support sums, but class sums remain unchanged.
        ++(m_splitSupSums[0]);
        --(m_splitSupSums[1]);

        // Compare with next value. If different, then compute score.
        if (pCurrValue->getRawValue() != pNextValue->getRawValue()) {
            if (!computeMaxHelper(m_splitSupSums, m_splitClassSums, *m_pSplitSupMatrix, max))
                return false;

			if (!computeInfoGainHelper(computeEntropy(&m_splitClassSums), m_splitSupSums, m_splitClassSums, *m_pSplitSupMatrix, infoGain))
                return false;
			
			cRanges.Add(new CTDRange(pNextValue->getRawValue(), pCurrValue->getRawValue()));  
			ranges.Add(pNextValue->getRawValue()- pCurrValue->getRawValue());
		
#if defined(_TD_SCORE_FUNTION_MAX) 
			weights.Add(max);
#endif

#if defined(_TD_SCORE_FUNCTION_INFOGAIN) 
			weights.Add(infoGain);
#endif


            FLAG = true;
        }
    }

	if (FLAG){

        // srand( (unsigned)time( NULL ) );
	
	    idx = expoMechSplit(epsilon, &weights, &ranges);
	    m_splitPoint = (float) (rand() % (int)(cRanges.GetAt(idx)->m_upperValue - cRanges.GetAt(idx)->m_lowerValue + 1) + cRanges.GetAt(idx)->m_lowerValue);	

	}
	else {

		// it means all the rawvlaues are same. Hence, the m_splitPoint should be chosen uniformly from the domian.
		// srand( (unsigned)time( NULL ) );
		m_splitPoint = (float) (rand() % (int)(m_upperBound - m_lowerBound + 1) + m_lowerBound);
	
    }
    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDContConcept::makeChildConcepts()
{
    // Make left child.
    CTDContConcept* pLeftConcept = new CTDContConcept(m_pAttrib);
    if (!pLeftConcept) {
        ASSERT(false);
        return false;
    }
    pLeftConcept->m_lowerBound = m_lowerBound;
    pLeftConcept->m_upperBound = m_splitPoint;
    pLeftConcept->m_flattenIdx = m_pAttrib->getFlattenConcepts()->Add(pLeftConcept);

    LPTSTR tempStr = FloatToStr(m_lowerBound, TD_CONTVALUE_NUMDEC);
    pLeftConcept->m_conceptValue = tempStr;
    pLeftConcept->m_conceptValue += _T("-");    
    delete [] tempStr;
    tempStr = FloatToStr(m_splitPoint, TD_CONTVALUE_NUMDEC);
    pLeftConcept->m_conceptValue += tempStr;
    delete [] tempStr;
    tempStr = NULL;    
    if (!addChildConcept(pLeftConcept))
        return false;

    // Make right child.
    CTDContConcept* pRightConcept = new CTDContConcept(m_pAttrib);
    if (!pRightConcept) {
        ASSERT(false);
        return false;
    }
    pRightConcept->m_lowerBound = m_splitPoint;
    pRightConcept->m_upperBound = m_upperBound;
    pRightConcept->m_flattenIdx = m_pAttrib->getFlattenConcepts()->Add(pRightConcept);

    tempStr = FloatToStr(m_splitPoint, TD_CONTVALUE_NUMDEC);
    pRightConcept->m_conceptValue = tempStr;
    pRightConcept->m_conceptValue += _T("-");
    delete [] tempStr;
    tempStr = FloatToStr(m_upperBound, TD_CONTVALUE_NUMDEC);
    pRightConcept->m_conceptValue += tempStr;
    delete [] tempStr;
    tempStr = NULL;
    if (!addChildConcept(pRightConcept))
        return false;

    return true;
}


//*****************
// CTDSetConcept *
//*****************


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDSetConcept::CTDSetConcept(CTDAttrib* pAttrib) 
    : CTDConcept(pAttrib)
{
	id = 0;

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDSetConcept::~CTDSetConcept() 
{
	
}

//---------------------------------------------------------------------------
// {Any Location {BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}}
// Set-Valued data and categorical data has same taxonomy tree format. Each concept however keeps a "min" and "max" variable. 
//---------------------------------------------------------------------------
bool CTDSetConcept::initHierarchy(LPCTSTR conceptStr, int depth, CTDIntArray& maxBranches)
{
    // Parse the conceptValue and the rest of the string.
    CString restStr;
    if (!parseConceptValue(conceptStr, m_conceptValue, restStr)) {
        cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << conceptStr << endl;
        return false;
    }
    m_depth = depth;

    // Depth-first build.
    CString firstConcept;
    while (!restStr.IsEmpty()) {
        if (!parseFirstConcept(firstConcept, restStr)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << restStr << endl;
            return false;
        }

        CTDSetConcept* pNewConcept = new CTDSetConcept(m_pAttrib);
        if (!pNewConcept)
            return false;
        
        if (!pNewConcept->initHierarchy(firstConcept, depth + 1, maxBranches)) {
            cerr << _T("CTDDiscConcept: Failed to build hierarchy from ") << firstConcept << endl;
            return false;
        }

        if (!addChildConcept(pNewConcept))
            return false;
    }

    // Update the maximum # of branches at this level
    int nChildren = getNumChildConcepts();
    if (nChildren > 0) {

		m_minimum = getChildConcept(0)->m_minimum;
		m_maximum = getChildConcept(nChildren-1)->m_maximum;
	}
	else{
	
		m_pAttrib->m_numLeafNodes++;
		id = m_pAttrib->m_numLeafNodes;
		m_minimum = m_pAttrib->m_numLeafNodes;
		m_maximum = m_pAttrib->m_numLeafNodes;
	}

    return true;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CString CTDSetConcept::toString()
{
    return m_conceptValue;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// static
bool CTDSetConcept::parseConceptValue(LPCTSTR str, CString& conceptVal, CString& restStr)
{
    conceptVal.Empty();
    restStr.Empty();

    CString wrkStr = str;
    if (wrkStr.GetLength() < 2 ||
        wrkStr[0] !=  TD_CONHCHY_OPENTAG || 
        wrkStr[wrkStr.GetLength() - 1] !=  TD_CONHCHY_CLOSETAG) {
        ASSERT(false);
        return false;
    }

    // Extract "Canada {BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}"
    wrkStr = wrkStr.Mid(1, wrkStr.GetLength() - 2);
    CBFStrHelper::trim(wrkStr);
    if (wrkStr.IsEmpty()) {
        ASSERT(false);
        return false;
    }

    // Extract "Canada"
    int openPos = wrkStr.Find(TD_CONHCHY_OPENTAG);
    if (openPos < 0) {
        // This is root value, e.g., "Vancouver".
        conceptVal = wrkStr;
        return true;
    }
    else {
        conceptVal = wrkStr.Left(openPos);
        CBFStrHelper::trim(conceptVal);
        if (conceptVal.IsEmpty()) {
            ASSERT(false);
            return false;
        }
    }

    // Extract "{BC {Vancouver} {Surrey} {Richmond}} {AB {Calgary} {Edmonton}}"
    restStr = wrkStr.Mid(openPos);
    return true;
}



//**************
// CTDHierarchyCuts *
//**************
CTDHierarchyCuts::CTDHierarchyCuts()
{
}

CTDHierarchyCuts::~CTDHierarchyCuts()
{
}

void CTDHierarchyCuts::cleanup()
{
    for (int i = 0; i < GetSize(); ++i)
        delete GetAt(i);

    RemoveAll();
}

CString CTDHierarchyCuts::toString()
{
	CString str;

	str += GetAt(0)->toString();
	for (int i = 1; i < GetSize(); ++i){
		str += _T(" ");
		str += GetAt(i)->toString();
	}
		
    return str;
}

//*************
// CTDHierarchyCut *
//*************

CTDHierarchyCut::CTDHierarchyCut(CTDAttrib* pAttrib)
    : m_pParentHierarchyCut(NULL), 
      m_pAttrib(pAttrib),
      m_childIdx(-1), 
      m_flattenIdx(-1)
{
    // use dynamic alocation in order to use forward declaration.
    m_pRelatedPartitions = new CTDPartitions();
	m_pTestRelatedPartitions = new CTDPartitions();

}

CTDHierarchyCut::~CTDHierarchyCut() 
{
    delete m_pRelatedPartitions;
    m_pRelatedPartitions = NULL;

	delete m_pTestRelatedPartitions;
	m_pTestRelatedPartitions = NULL;
    
	m_childHierarchyCuts.cleanup();
}

CString CTDHierarchyCut::toString()
{
    return m_conceptsValues.toString();
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDHierarchyCut::addChildConcept(CTDHierarchyCut* pChildHierarchyCut, int childIdx)
{
    try {
        pChildHierarchyCut->m_pParentHierarchyCut = this;
        pChildHierarchyCut->m_childIdx = childIdx;
		m_childHierarchyCuts.Add(pChildHierarchyCut);
        return true;
    }
    catch (CMemoryException&) {
        ASSERT(false);
        return false;
    }
}

//---------------------------------------------------------------------------
// Replaces the "pConcept" of the parent's "m_conceptsValues" with the "pNewConcetps" 
//---------------------------------------------------------------------------
bool CTDHierarchyCut::setConceptsValues(CTDConcept* pConcept, CTDConcepts* pNewConcetps)
{
   
	try {
		CTDHierarchyCut* pParentHierarchyCut = getParentConcept();
		for (int i = 0; i < pParentHierarchyCut->m_conceptsValues.GetSize(); i++){
			if (pParentHierarchyCut->m_conceptsValues.GetAt(i)->m_conceptValue.CompareNoCase(pConcept->m_conceptValue) == 0){
				m_conceptsValues.Append(*pNewConcetps);
			}else{
				m_conceptsValues.Add(pParentHierarchyCut->m_conceptsValues.GetAt(i));
			}
		}
        return true;
    }
    catch (CMemoryException&) {
        ASSERT(false);
        return false;
    }
}

//---------------------------------------------------------------------------
// Generate the concept values of the HierarchyCut from the child Index.
//---------------------------------------------------------------------------
CTDConcepts* CTDHierarchyCut::getChildValues(int childIdx, CTDConcepts* childConcepts)
{
	int SIZE =  childConcepts->GetSize();
	int* arr = new int[SIZE];
	for(int i=0; i<SIZE; i++)
		arr[i] = 0;
	
	CTDConcepts* pConceptValues = new CTDConcepts ();

	int cindex = 0;

	while(childIdx>0){
		arr[cindex] = childIdx%2;
		childIdx/=2;
		cindex++;
	}

	for(int i=0; i<=SIZE; i++)
		if(arr[i]==1)
			pConceptValues->Add(childConcepts->GetAt(i));

	delete[] arr;
	return pConceptValues;

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool CTDHierarchyCut::registerHierarchyCut()
{ 
	CTDConcept* pConceptValue = NULL;
	for (int i =0; i < m_conceptsValues.GetSize(); i++){
		pConceptValue = m_conceptsValues.GetAt(i);
		pConceptValue->registerHierarchyCut(this);
	}
	return true; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int CTDHierarchyCut::getNumChildConcepts() const
{ 
    return m_childHierarchyCuts.GetSize(); 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDHierarchyCut* CTDHierarchyCut::getChildHierarchyCut(int idx) const
{
	for (int i = 0; i < m_childHierarchyCuts.GetSize(); i++){
		if (m_childHierarchyCuts.GetAt(i)->m_childIdx == idx)
			return m_childHierarchyCuts.GetAt(i); 
	}
	return NULL;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDHierarchyCut* CTDHierarchyCut::getParentConcept() 
{ 
    return m_pParentHierarchyCut; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartitions* CTDHierarchyCut::getRelatedPartitions() 
{ 
    return m_pRelatedPartitions; 
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CTDPartitions* CTDHierarchyCut::getTestRelatedPartitions() 
{ 
    return m_pTestRelatedPartitions; 
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
POSITION CTDHierarchyCut::registerPartition(CTDPartition* pPartition) 
{ 
    return m_pRelatedPartitions->AddTail(pPartition);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
POSITION CTDHierarchyCut::testRegisterPartition(CTDPartition* pPartition) 
{ 
    return m_pTestRelatedPartitions->AddTail(pPartition);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CTDHierarchyCut::deregisterPartition(POSITION pos) 
{
    m_pRelatedPartitions->RemoveAt(pos);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void CTDHierarchyCut::testDeregisterPartition(POSITION pos) 
{
    m_pTestRelatedPartitions->RemoveAt(pos);
}

//**************
// CTDRanges *
//**************
CTDRanges::CTDRanges()
{
}

CTDRanges::~CTDRanges()
{
}

void CTDRanges::cleanup()
{
    for (int i = 0; i < GetSize(); ++i)
        delete GetAt(i);

    RemoveAll();
}

//**************
// CTDRange *
//**************
CTDRange::CTDRange(float upperValue, float lowerValue)
      : m_upperValue(upperValue), 
        m_lowerValue(lowerValue)
{
}

CTDRange::~CTDRange()
{
}
