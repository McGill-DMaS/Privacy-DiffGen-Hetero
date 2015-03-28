// TDValue.h: interface for the CTDValue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(TDVALUE_H)
#define TDVALUE_H

#if !defined(TDATTRIBUTE_H)
    #include "TDAttribute.h"
#endif

#if !defined(TDCONCEPT_H)
    #include "TDConcept.h"
#endif


class CTDValue
{
public:
    CTDValue();
    virtual ~CTDValue();

// operations
    bool initConceptToRoot(CTDAttrib* pAttrib);
    bool initConceptToLevel1(CTDAttrib* pAttrib);
	bool setSplitCurHierarchyCut(CTDHierarchyCut* pSplitHierarchyCut, int childInd);
	bool setSplitCurConcept(CTDConcept* pSplitConcept,int childInd);
	bool setCurHierarchyCut(CTDHierarchyCut* pCurHierarchyCut);
	bool setCurConcept(CTDConcept* pCurConcept);
    bool assignGenClassValue(CTDAttrib* pAttrib, int classInd);
	bool lowerCurrentConcept();

    virtual CString toString(bool bRawValue) = 0;
    virtual bool buildBitValue(const CString& rawVal, CTDAttrib* pAttrib) = 0;
	virtual CTDConcept* getLowerConcept() = 0;
	virtual bool assignRawConcept(CTDAttrib* pAttrib, int classInd) = 0;

	virtual CTDConcepts* getRawConcepts() = 0 ; //only defined for set-valued data
    int getCurrentConceptFlattenIdx() { return m_pCurrConcept->m_flattenIdx; };
    CTDConcept* getCurrentConcept() { return m_pCurrConcept; };  
	CTDHierarchyCut* getCurrentHierarchyCut() { return m_pCurrHierarchyCut; };


protected:
// attributes
    CTDHierarchyCut* m_pCurrHierarchyCut;     // Pointer to the current concepts for set value
	CTDConcept* m_pCurrConcept;				  // Pointer to the current concept.
};


class CTDStringValue : public CTDValue
{
public:
    CTDStringValue() : m_bitValue(0), m_pRawConcept(NULL) {};
    virtual ~CTDStringValue() {};
    
    virtual CString toString(bool bRawValue);
    virtual bool buildBitValue(const CString& rawVal, CTDAttrib* pAttrib);
    virtual CTDConcept* getLowerConcept();    
    CTDConcept* getRawConcept();
	virtual CTDConcepts* getRawConcepts() { return NULL;} ; //only defined for set-valued data
	virtual bool assignRawConcept(CTDAttrib* pAttrib, int classInd);

// static functions
    static bool buildConceptPath(const CString& rawVal, CTDConcepts* pFlatten, CTDConcepts& conceptPath);

protected:
    CTDConcept* getLowerConceptGenMode(CTDConcept* pThisConcept);
    CTDConcept* getLowerConceptSupMode();

// attributes
    UINT m_bitValue;                    // UINT to make sure shift in 0 in case cross-platform.
    CTDConcept* m_pRawConcept;          // Pointer to the raw concept.
};


class CTDNumericValue : public CTDValue
{
public:
    CTDNumericValue(float val) : m_numValue(val) {};
    virtual ~CTDNumericValue() {};

    float getRawValue() { return m_numValue; };
	virtual CTDConcepts* getRawConcepts() { return NULL;} ; //only defined for set-valued data
    bool insertConcept(CTDContConcept* pConcept);

    virtual CString toString(bool bRawValue); 
    virtual bool buildBitValue(const CString& rawVal, CTDAttrib* pAttrib) { return true; };
    virtual CTDConcept* getLowerConcept();
	virtual bool assignRawConcept(CTDAttrib* pAttrib, int classInd) { return true; };


protected:
// attributes
    float m_numValue; 
};

class CTDSetValue : public CTDValue
{
public:
    CTDSetValue();
    virtual ~CTDSetValue() {};
  
    virtual CString toString(bool bRawValue);
    virtual bool buildBitValue(const CString& rawVal, CTDAttrib* pAttrib);
	virtual CTDConcept* getLowerConcept() { return NULL; };    
	virtual CTDConcepts* getRawConcepts(){ return &m_pRawConcepts; };
	virtual bool assignRawConcept(CTDAttrib* pAttrib, int classInd){ return true; };

protected:
	// attributes
	CTDConcepts m_pRawConcepts;          // Pointer to the raw concepts.
};


typedef CTypedPtrArray<CPtrArray, CTDValue*> CTDValueArray;
class CTDValues : public CTDValueArray
{
public:
    CTDValues();
    virtual ~CTDValues();
    void cleanup();
};

#endif
