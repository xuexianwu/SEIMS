/*----------------------------------------------------------------------
*	Purpose: 	Channel routing using implicit finite difference method (kinematic wave method in the LISEM model)
*
*	Created:	Junzhi Liu
*	Date:		23-Febrary-2011
*
*	Revision:
*   Date:
*---------------------------------------------------------------------*/
//#include "vld.h"
#include "IKW_CH.h"
#include "MetadataInfo.h"
#include "ModelException.h"
#include "util.h"
#include <omp.h>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>

#define MINI_SLOPE 0.0001f
#define NODATA_VALUE -9999

#define MIN_FLUX 1e-12f 
#define MAX_ITERS_CH 10

const float _23 = 2.0f/3.0f;
const float SQ2 = sqrt(2.f);

using namespace std;

ImplicitKinematicWave::ImplicitKinematicWave(void):m_size(-1), m_chNumber(-1), m_dt(-1.0f), m_dx(-1.0f),
	m_sRadian(NULL), m_direction(NULL), m_reachDownStream(NULL), m_chWidth(NULL), 
	m_qs(NULL), m_hCh(NULL), m_qCh(NULL), m_prec(NULL), m_qSubbasin(NULL), m_qg(NULL),
	m_flowLen(NULL), m_qi(NULL), m_streamLink(NULL), m_reachId(NULL), m_sourceCellIds(NULL),
	m_idUpReach(-1), m_qUpReach(0.f), m_manningScalingFactor(0.4f), m_qgDeep(100.f), m_idOutlet(-1)//, m_qsInput(NULL)
{
	//m_diagonal[1] = 0;
	//m_diagonal[2] = 1;
	//m_diagonal[4] = 0;
	//m_diagonal[8] = 1;
	//m_diagonal[16] = 0;
	//m_diagonal[32] = 1;
	//m_diagonal[64] = 0;
	//m_diagonal[128] = 1;

	m_diagonal[1] = 0;
	m_diagonal[2] = 1;
	m_diagonal[3] = 0;
	m_diagonal[4] = 1;
	m_diagonal[5] = 0;
	m_diagonal[6] = 1;
	m_diagonal[7] = 0;
	m_diagonal[8] = 1;

	
}


ImplicitKinematicWave::~ImplicitKinematicWave(void)
{
	if (m_hCh != NULL)
	{
		for (int i = 0; i < m_chNumber; ++i)
			delete[] m_hCh[i];
		delete[] m_hCh;
	}


	if (m_qCh != NULL)
	{
		for (int i = 0; i < m_chNumber; ++i)
			delete[] m_qCh[i];
		delete[] m_qCh;
	}

	if (m_flowLen != NULL)
	{
		for (int i = 0; i < m_chNumber; ++i)
			delete[] m_flowLen[i];
		delete[] m_flowLen;
	}

	if (m_sourceCellIds != NULL)
		delete[] m_sourceCellIds;

	if (m_qSubbasin != NULL)
		delete[] m_qSubbasin;

	//if (m_qsInput != NULL)
	//	delete[] m_qsInput;
}

//---------------------------------------------------------------------------
// modified from OpenLISEM
/** Newton Rapson iteration for new water flux in cell, based on Ven Te Chow 1987
\param qIn      summed Q new from upstream
\param qLast    current discharge in the cell
\param surplus        infiltration surplus flux (in m2/s), has value <= 0
\param alpha    alpha calculated in LISEM from before kinematic wave
\param dt   timestep
\param dx   length of the cell corrected for slope
*/
float ImplicitKinematicWave::GetNewQ(float qIn, float qLast, float surplus, float alpha, float dt, float dx)
{
    /* Using Newton-Raphson Method */
    float  ab_pQ, dtX, C;  //auxillary vars
    int   count;
    float Qkx; //iterated discharge, becomes Qnew
    float fQkx; //function
    float dfQkx;  //derivative
    const float _epsilon = 1e-12f;
    const float beta = 0.6f;

    /* if no input then output = 0 */
    if ((qIn + qLast) <= -surplus*dx)//0)
    {
        //itercount = -1;
        return(0);
    }

    /* common terms */
    ab_pQ = alpha*beta*pow(((qLast+qIn)/2),beta-1);
    // derivative of diagonal average (space-time)

    dtX = dt/dx;
    C = dtX*qIn + alpha*pow(qLast,beta) + dt*surplus;
    //dt/dx*Q = m3/s*s/m=m2; a*Q^b = A = m2; surplus*dt = s*m2/s = m2
    //C is unit volume of water
    // first gues Qkx
    Qkx   = (dtX * qIn + qLast * ab_pQ + dt * surplus) / (dtX + ab_pQ);

    // VJ 050704, 060830 infil so big all flux is gone
    //VJ 110114 without this de iteration cannot be solved for very small values
    if (Qkx < MIN_FLUX)
    {
        //itercount = -2;
        return(0);
    }

    Qkx   = max(Qkx, MIN_FLUX);

    count = 0;
    do {
        fQkx  = dtX * Qkx + alpha * pow(Qkx, beta) - C;   /* Current k */
        dfQkx = dtX + alpha * beta * pow(Qkx, beta - 1);  /* Current k */
        Qkx   -= fQkx / dfQkx;                                /* Next k */
        Qkx   = max(Qkx, MIN_FLUX);
        count++;
        //qDebug() << count << fQkx << Qkx;
    } while(fabs(fQkx) > _epsilon && count < MAX_ITERS_CH);

	if(Qkx != Qkx)
		throw ModelException("IKW_OL", "GetNewQ", "Error in iteration!");

    //itercount = count;
    return Qkx;
}

// end code form LISEM 

bool ImplicitKinematicWave::CheckInputData(void)
{
	if(m_date <= 0)
		throw ModelException("IKW_CH","CheckInputData","You have not set the Date variable.");

	if(m_size <= 0)
		throw ModelException("IKW_CH","CheckInputData","The cell number of the input can not be less than zero.");

	if(m_dt <= 0)
		throw ModelException("IKW_CH","CheckInputData","You have not set the TimeStep variable.");

	if(m_dx <= 0)
		throw ModelException("IKW_CH","CheckInputData","You have not set the CellWidth variable.");

	if(m_sRadian == NULL)
		throw ModelException("IKW_CH","CheckInputData","The parameter: RadianSlope has not been set.");
	if(m_direction == NULL)
		throw ModelException("IKW_CH","CheckInputData","The parameter: flow direction has not been set.");
	
	if(m_chWidth == NULL)
		throw ModelException("IKW_CH","CheckInputData","The parameter: CHWIDTH has not been set.");
	if(m_streamLink == NULL)
		throw ModelException("IKW_CH","CheckInputData","The parameter: STREAM_LINK has not been set.");

	if(m_prec == NULL)
		throw ModelException("IKW_CH","CheckInputData","The parameter: D_P(precipitation) has not been set.");

	return true;
}

void  ImplicitKinematicWave::initalOutputs()
{
	if(m_size <= 0) throw ModelException("IKW_CH","initalOutputs","The cell number of the input can not be less than zero.");

	if(m_hCh == NULL)
	{
		// find source cells the reaches
		m_sourceCellIds = new int[m_chNumber];
		//m_qsInput = new float[m_chNumber+1];

		for (int i = 0; i < m_chNumber; ++i)
		{
			m_sourceCellIds[i] = -1;
			//m_qsInput[i] = 0.f;
		}

		for (int i = 0; i < m_size; i++)
		{
			if (FloatEqual(m_streamLink[i], NODATA_VALUE))
				continue;
			int reachId = (int)m_streamLink[i];
			bool isSource = true;
			for (int k = 1; k <= (int)m_flowInIndex[i][0]; ++k)
			{
				int flowInId = (int)m_flowInIndex[i][k];
				int flowInReachId = (int)m_streamLink[flowInId];
				if (flowInReachId == reachId)
				{
					isSource = false;
					break;
				}
			}

			if((int)m_flowInIndex[i][0] == 0)
				isSource = true;

			if(isSource)
			{
				int reachIndex = m_idToIndex[reachId];
				m_sourceCellIds[reachIndex] = i;
			}
		}

		//for(int i = 0; i < m_chNumber; i++)
		//	cout << m_sourceCellIds[i] << endl;

		// get the cells in reaches according to flow direction
		for (int iCh = 0; iCh < m_chNumber; iCh++)
		{
			int iCell = m_sourceCellIds[iCh];
			int reachId = (int)m_streamLink[iCell];
			while ((int)m_streamLink[iCell] == reachId)
			{
				m_reachs[iCh].push_back(iCell);
				iCell = (int)m_flowOutIndex[iCell];
			}
		}

		if (m_reachLayers.empty())
		{
			for (int i = 0; i < m_chNumber; i++)
			{
				int order = (int)m_streamOrder[i];
				m_reachLayers[order].push_back(i);
			}
		}
		m_hCh = new float *[m_chNumber];
		m_qCh = new float *[m_chNumber];

		//m_flowLen = new float *[m_chNumber];
		
		m_qSubbasin = new float[m_chNumber];
		for (int i = 0; i < m_chNumber; ++i)
		{
			int n = m_reachs[i].size();
			m_hCh[i] = new float[n];
			m_qCh[i] = new float[n];

			//m_flowLen[i] = new float[n];
			
			m_qSubbasin[i] = 0.f;

			//int id;
			//float dx;
			for (int j = 0; j < n; ++j)
			{
				m_hCh[i][j] = 0.f;
				m_qCh[i][j] = 0.f;

				//id = m_reachs[i][j];
				//
				//// slope length needs to be corrected by slope angle
				//dx = m_dx / cos(m_sRadian[id]);
				//int dir = (int)m_direction[id];
				//if ((int)m_diagonal[dir] == 1)
				//	dx = SQ2*dx;
				//m_flowLen[i][j] = dx;
 
			}
		}

	}

}

void  ImplicitKinematicWave::initalOutputs2()
{
	if(m_flowLen != NULL)
		return;

	m_flowLen = new float *[m_chNumber];

	for (int i = 0; i < m_chNumber; ++i)
	{
		int n = m_reachs[i].size();
		m_flowLen[i] = new float[n];

		int id;
		float dx;
		for (int j = 0; j < n; ++j)
		{
			id = m_reachs[i][j];
			// slope length needs to be corrected by slope angle
			dx = m_dx / cos(m_sRadian[id]);
			int dir = (int)m_direction[id];
			if ((int)m_diagonal[dir] == 1)
				dx = SQ2*dx;
			m_flowLen[i][j] = dx;
		}
	}
}

void ImplicitKinematicWave::ChannelFlow(int iReach, int iCell, int id, float qgEachCell)
{
	float qUp = 0.f;

	if (iReach == 0 && iCell == 0)
	{
		qUp = m_qUpReach;
	}

	// inflow from upstream channel
	if (iCell == 0)// inflow of this cell is the last cell of the upstream reach
	{
		for (size_t i = 0; i < m_reachUpStream[iReach].size(); ++i)
		{
			int upReachId = m_reachUpStream[iReach][i];
			if (upReachId >= 0)
			{
				int upCellsNum = m_reachs[upReachId].size();
				int upCellId = m_reachs[upReachId][upCellsNum-1];
				qUp += m_qCh[upReachId][upCellsNum-1];
			}
		}
		//cout << qUp << "\t";
	}
	else
	{
		qUp = m_qCh[iReach][iCell-1] ;
	}

	float dx = m_flowLen[iReach][iCell];

	float qLat = m_prec[id]/1000.f * m_chWidth[id] * dx / m_dt;
	qLat += qgEachCell;
	
	//if (m_qs != NULL)
	qLat += m_qs[id];
	if (m_qi != NULL)
		qLat += m_qi[id];

	if (qLat < MIN_FLUX && qUp < MIN_FLUX)
	{
		m_hCh[iReach][iCell] = 0.f;
		m_qCh[iReach][iCell] = 0.f;
		return;
	}

	qUp += qLat;

	float Perim = 2.f*m_hCh[iReach][iCell] + m_chWidth[id];
	
	float sSin = sqrt(sin(m_sRadian[id]));
	float alpha = pow((m_manningScalingFactor*m_reachN[iReach])/sSin * pow(Perim, _23), 0.6f);

	float qIn = m_qCh[iReach][iCell];
	
	m_qCh[iReach][iCell] = GetNewQ(qUp, qIn, 0.f, alpha, m_dt, dx);

	float hTest = m_hCh[iReach][iCell] + (qUp - m_qCh[iReach][iCell])*m_dt/m_chWidth[id]/dx;
	float hNew = (alpha*pow(m_qCh[iReach][iCell], 0.6f)) / m_chWidth[id]; // unit m
	m_hCh[iReach][iCell] = (alpha*pow(m_qCh[iReach][iCell], 0.6f)) / m_chWidth[id]; // unit m
	

}

int ImplicitKinematicWave::Execute()
{
	//check the data
	CheckInputData();	

	initalOutputs();
	initalOutputs2();
	//Output1DArray(m_size, m_prec, "f:\\p2.txt");
	map<int, vector<int> >::iterator it;
	//cout << m_reachLayers.size() << "\t" << m_chNumber << endl;

	for (it = m_reachLayers.begin(); it != m_reachLayers.end(); it++)
	{
		// There are not any flow relationship within each routing layer.
		// So parallelization can be done here.
		int nReaches = it->second.size();
		//cout << "Number of reaches: " << nReaches << endl;
		// the size of m_reachLayers (map) is equal to the maximum stream order
		#pragma omp parallel for
		for (int i = 0; i < nReaches; ++i)
		{
			int reachIndex = it->second[i]; // index in the array, from 0
			//m_qsInput[reachIndex+1] = 0.f;

			vector<int>& vecCells = m_reachs[reachIndex];
			int n = vecCells.size();
			//cout << "\tNumber of cells in reach " << reachIndex << ": " << n << endl;
			float qgEachCell = 0.f;
			if(m_qg != NULL)
				qgEachCell = m_qg[i+1]/n;
			//cout << "\tGroundwater: " << qgEachCell << endl;
			for (int iCell = 0; iCell < n; ++iCell)
			{
				int idCell = vecCells[iCell];
				//m_qsInput[reachIndex+1] += m_qs[idCell];
				ChannelFlow(reachIndex, iCell, idCell, qgEachCell);
			}
			m_qSubbasin[reachIndex] = m_qCh[reachIndex][n-1];
		}
	}
	//cout << endl;

	//m_qsInput[0] = 0.f;
	//for(int i = 1; i <= m_chNumber; i++)
	//	m_qsInput[0] += m_qsInput[i];

	return 0;
}

bool ImplicitKinematicWave::CheckInputSize(const char* key, int n)
{
	if(n <= 0)
	{
		//StatusMsg("Input data for "+string(key) +" is invalid. The size could not be less than zero.");
		return false;
	}
	if(m_size != n)
	{
		if(m_size <=0) m_size = n;
		else
		{
			//StatusMsg("Input data for "+string(key) +" is invalid. All the input data should have same size.");
			ostringstream oss;
			oss << "Input data for "+string(key) << " is invalid with size: " << n << ". The origin size is " << m_size << ".\n";  
			throw ModelException("IKW_CH","CheckInputSize",oss.str());
		}
	}

	return true;
}

bool ImplicitKinematicWave::CheckInputSizeChannel(const char* key, int n)
{
	if(n <= 0)
	{
		//StatusMsg("Input data for "+string(key) +" is invalid. The size could not be less than zero.");
		return false;
	}
	if(m_chNumber != n)
	{
		if(m_chNumber <=0) m_chNumber = n;
		else
		{
			//StatusMsg("Input data for "+string(key) +" is invalid. All the input data should have same size.");
			return false;
		}
	}

	return true;
}

void ImplicitKinematicWave::GetValue(const char* key, float* value)
{
	string sk(key);
	if (StringMatch(sk, "QOUTLET"))
	{
		map<int, vector<int> >::iterator it = m_reachLayers.end();
		it--;
		int reachId = it->second[0];
		int iLastCell = m_reachs[reachId].size() - 1;
		*value = m_qCh[reachId][iLastCell];
		//*value = m_hToChannel[m_idOutlet];
		//*value = m_qs[m_idOutlet];
		//*value = m_qs[m_idOutlet] + m_qCh[reachId][iLastCell];
	}
	else if (StringMatch(sk, "QTotal"))
	{
		map<int, vector<int> >::iterator it = m_reachLayers.end();
		it--;
		int reachId = it->second[0];
		int iLastCell = m_reachs[reachId].size() - 1;
		*value = m_qCh[reachId][iLastCell] + m_qgDeep;
		//*value = m_hToChannel[m_idOutlet];
		//*value = m_qs[m_idOutlet];
		//*value = m_qs[m_idOutlet] + m_qCh[reachId][iLastCell];
	}

}

void ImplicitKinematicWave::SetValue(const char* key, float data)
{
	string sk(key);
	if (StringMatch(sk, "DT_HS"))
		m_dt = data;
	else if (StringMatch(sk, "CellWidth"))
		m_dx = data;
	else if (StringMatch(sk, "ID_UPREACH"))
		m_idUpReach = (int)data;
	else if(StringMatch(sk, "QUPREACH"))
		m_qUpReach = data;
	else if (StringMatch(sk, "ThreadNum"))
	{
		omp_set_num_threads((int)data);
	}
	else if (StringMatch(sk, "CH_ManningFactor"))
		m_manningScalingFactor = data;	
	else
		throw ModelException("IKW_CH", "SetValue", "Parameter " + sk 
		+ " does not exist. Please contact the module developer.");

}

void ImplicitKinematicWave::Set1DData(const char* key, int n, float* data)
{
	string sk(key);

	if (StringMatch(sk, "SBQG"))
	{
		m_qg = data;
		return;
	}

	//check the input data
	CheckInputSize(key,n);
	
	if(StringMatch(sk, "RadianSlope"))
		m_sRadian = data;
	else if(StringMatch(sk, "Flow_Dir"))
		m_direction = data;
	else if(StringMatch(sk, "D_P"))
		m_prec = data;
	else if(StringMatch(sk, "D_QSoil"))
		m_qi = data;
	else if(StringMatch(sk, "D_QOverland"))
		m_qs = data;
	else if(StringMatch(sk, "Manning_nCh"))
		m_reachN = data;
	else if(StringMatch(sk, "CHWIDTH"))
		m_chWidth = data;
	else if(StringMatch(sk, "STREAM_LINK"))
		m_streamLink = data;
	else if(StringMatch(sk, "FlowOut_Index_D8"))
	{
		m_flowOutIndex = data;
		for (int i = 0; i < m_size; i++)
		{
			if (m_flowOutIndex[i] < 0)
			{
				m_idOutlet = i;
				break;
			}
		}
	}
	else
		throw ModelException("IKW_CH", "Set1DData", "Parameter " + sk 
		+ " does not exist. Please contact the module developer.");
	

}

void ImplicitKinematicWave::Get1DData(const char* key, int* n, float** data)
{
	string sk(key);
	//*n = m_size;
	*n = m_chNumber;
	if (StringMatch(sk, "QRECH"))
	{
		*data = m_qSubbasin;
	}
	//else if(StringMatch(sk,"SBOF_IKW"))
	//{
	//	*data = m_qsInput;
	//}
	/*else if (StringMatch(sk, "CHWATH"))
	{
	*data = m_chwath;
	}
	else if (StringMatch(sk, "CHQCH"))
	{
	*data = m_chwath;
	}*/
	else
		throw ModelException("IKW_CH", "Get1DData", "Output " + sk 
		+ " does not exist in the IKW_CH module. Please contact the module developer.");

}

void ImplicitKinematicWave::Get2DData(const char* key, int *nRows, int *nCols, float*** data)
{
	if(m_hCh == NULL || m_qCh == NULL)
		initalOutputs();
	string sk(key);
	*nRows = m_chNumber;
	if (StringMatch(sk, "QRECH"))
		*data = m_qCh;
	else if (StringMatch(sk, "HCH"))
		*data = m_hCh;
	else
		throw ModelException("IKW_CH", "Get2DData", "Output " + sk 
		+ " does not exist in the IKW_CH module. Please contact the module developer.");

}

void ImplicitKinematicWave::Set2DData(const char* key, int nrows, int ncols, float** data)
{
	string sk(key);
	if(StringMatch(sk, "ReachParameter"))
	{
		//cout << "Set2DData: " << nrows << "\t" << ncols << endl;

		m_chNumber = ncols;   // overland is nrows;
		m_reachId = data[0];
		m_streamOrder = data[1];
		m_reachDownStream = data[2];
		m_reachN = data[3];
		for (int i = 0; i < m_chNumber; i++)
			m_idToIndex[(int)m_reachId[i]] = i;
		
		m_reachUpStream.resize(m_chNumber);
		for (int i = 0; i < m_chNumber; i++)
		{
			int downStreamId = int(m_reachDownStream[i]);
			if(downStreamId <= 0)
				continue;
			if (m_idToIndex.find(downStreamId) != m_idToIndex.end())
			{
				int downStreamIndex = m_idToIndex.at(downStreamId);
				m_reachUpStream[downStreamIndex].push_back(i);
			}
		}

	}
	else if (StringMatch(sk, "FlowIn_Index_D8"))
		m_flowInIndex = data;
	else
		throw ModelException("IKW_CH", "Set1DData", "Parameter " + sk 
			+ " does not exist. Please contact the module developer.");
	
}
