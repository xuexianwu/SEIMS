/*----------------------------------------------------------------------
*	Purpose: 	A simple fill and spill method method to calculate depression storage
*
*	Created:	Junzhi Liu
*	Date:		14-Febrary-2011
*
*	Revision:	Zhiqiang Yu	
*   Date:		2011-2-15
*	Description:
*	1.	Modify the name of some parameters, input and output variables.
*		Please see the metadata rules for the names.
*	2.	Depre_in would be DT_Single. Add function SetSingleData() to 
*		set its value.
*	3.	This module will be called by infiltration module to get the 
*		depression storage. And this module will also use the outputs
*		of infiltration module. The sequence of this two modules is 
*		infiltration->depression. When infiltration first calls the 
*		depression module, the execute function of depression module
*		is not executed before getting the outputs. So, the output 
*		variables should be initial in the Get1DData function. This 
*		initialization is realized by function initalOutputs. 
*	4.	Delete input D_INFIL and add input D_EXCP.
*---------------------------------------------------------------------*/

#pragma once
#include <string>
#include <ctime>
#include "api.h"

using namespace std;
#include "SimulationModule.h"

class DepressionFSDaily:public SimulationModule
{
public:
	DepressionFSDaily(void);
	~DepressionFSDaily(void);

	virtual int Execute();
	virtual void SetValue(const char* key, float data);
	virtual void Set1DData(const char* key, int n, float* data);
	virtual void Get1DData(const char* key, int* n, float** data);
	
	bool CheckInputSize(const char* key, int n);
	bool CheckInputData(void);


private:
	/// size
	int m_size;

	/// initial depression storage coefficient
	float m_depCo;
	/// depression storage capacity
	float* m_depCap;

	/// pet
	float* m_pet;
	/// evaporation from the interception storage
	float* m_ei;

	/// Infiltration
	///float* m_infil;
	float* m_pe;

	// state variables (output)
	/// depression storage
	float* m_sd;
	/// evaporation from depression storage
	float* m_ed;
	/// surface runoff
	float* m_sr;


	void initalOutputs();
};

