/*
*	Copyright (C) 2008-2012 Thorsten Liebig (Thorsten.Liebig@gmx.de)
*
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU Lesser General Public License as published
*	by the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU Lesser General Public License for more details.
*
*	You should have received a copy of the GNU Lesser General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tinyxml.h"
#include <hdf5.h>
#include <hdf5_hl.h>

#include "ParameterCoord.h"
#include "CSPropDiscMaterial.h"

CSPropDiscMaterial::CSPropDiscMaterial(ParameterSet* paraSet) : CSPropMaterial(paraSet)
{
	Type=(CSProperties::PropertyType)(DISCRETE_MATERIAL | MATERIAL);
	Init();
}

CSPropDiscMaterial::CSPropDiscMaterial(CSProperties* prop) : CSPropMaterial(prop)
{
	Type=(CSProperties::PropertyType)(DISCRETE_MATERIAL | MATERIAL);
	Init();
}

CSPropDiscMaterial::CSPropDiscMaterial(unsigned int ID, ParameterSet* paraSet) : CSPropMaterial(ID, paraSet)
{
	Type=(CSProperties::PropertyType)(DISCRETE_MATERIAL | MATERIAL);
	Init();
}

CSPropDiscMaterial::~CSPropDiscMaterial()
{
	for (int n=0;n<3;++n)
	{
		delete[] m_mesh[n];
		m_mesh[n]=NULL;
	}
	delete[] m_Disc_Ind;
	m_Disc_Ind=NULL;
	delete[] m_Disc_epsR;
	m_Disc_epsR=NULL;
	delete[] m_Disc_kappa;
	m_Disc_kappa=NULL;
	delete[] m_Disc_mueR;
	m_Disc_mueR=NULL;
	delete[] m_Disc_sigma;
	m_Disc_sigma=NULL;
	delete[] m_Disc_Density;
	m_Disc_Density=NULL;

	delete m_Transform;
	m_Transform=NULL;
}

unsigned int CSPropDiscMaterial::GetWeightingPos(const double* inCoords)
{
	double coords[3];
	TransformCoordSystem(inCoords, coords, coordInputType, CARTESIAN);
	if (m_Transform)
		m_Transform->InvertTransform(coords,coords);
	for (int n=0;n<3;++n)
		coords[n]/=m_Scale;
	unsigned int pos[3];
	if (!(m_mesh[0] && m_mesh[1] && m_mesh[2]))
		return -1;
	for (int n=0;n<3;++n)
	{
		if (coords[n]<m_mesh[n][0])
			return -1;
		if (coords[n]>m_mesh[n][m_Size[n]-1])
			return -1;
		pos[n]=0;
		for (unsigned int i=0;i<m_Size[n];++i)
		{
			if (coords[n]<m_mesh[n][i])
			{
				pos[n]=i;
				break;
			}
		}
	}
	return pos[0] + pos[1]*m_Size[0] + pos[2]*m_Size[0]*m_Size[1];
}

int CSPropDiscMaterial::GetDBPos(const double* coords)
{
	if (m_Disc_Ind==NULL)
		return -1;
	unsigned int pos = GetWeightingPos(coords);
	if (pos==(unsigned int)-1)
		return -1;
	int db_pos = m_Disc_Ind[pos];
	if (db_pos>=(int)m_DB_size)
	{
		//sanity check, this should not happen!!!
		cerr << __func__ << ": Error, false DB position!" << endl;
		return -1;
	}
	return db_pos;
}

double CSPropDiscMaterial::GetEpsilonWeighted(int ny, const double* inCoords)
{
	if (m_Disc_epsR==NULL)
		return CSPropMaterial::GetEpsilonWeighted(ny,inCoords);
	int pos = GetDBPos(inCoords);
	if (pos<0)
		return CSPropMaterial::GetEpsilonWeighted(ny,inCoords);
	return m_Disc_epsR[pos];
}

double CSPropDiscMaterial::GetKappaWeighted(int ny, const double* inCoords)
{
	if (m_Disc_kappa==NULL)
		return CSPropMaterial::GetKappaWeighted(ny,inCoords);
	int pos = GetDBPos(inCoords);
	if (pos<0)
		return CSPropMaterial::GetKappaWeighted(ny,inCoords);
	return m_Disc_kappa[pos];
}

double CSPropDiscMaterial::GetMueWeighted(int ny, const double* inCoords)
{
	if (m_Disc_mueR==NULL)
		return CSPropMaterial::GetMueWeighted(ny,inCoords);
	int pos = GetDBPos(inCoords);
	if (pos<0)
		return CSPropMaterial::GetMueWeighted(ny,inCoords);
	return m_Disc_mueR[pos];
}

double CSPropDiscMaterial::GetSigmaWeighted(int ny, const double* inCoords)
{
	if (m_Disc_sigma==NULL)
		return CSPropMaterial::GetSigmaWeighted(ny,inCoords);
	int pos = GetDBPos(inCoords);
	if (pos<0)
		return CSPropMaterial::GetSigmaWeighted(ny,inCoords);
	return m_Disc_sigma[pos];
}

double CSPropDiscMaterial::GetDensityWeighted(const double* inCoords)
{
	if (m_Disc_Density==NULL)
		return CSPropMaterial::GetDensityWeighted(inCoords);
	int pos = GetDBPos(inCoords);
	if (pos<0)
		return CSPropMaterial::GetDensityWeighted(inCoords);
	return m_Disc_Density[pos];
}

void CSPropDiscMaterial::Init()
{
	m_Filename.clear();
	m_FileType=-1;

	m_DB_size = 0;

	for (int n=0;n<3;++n)
		m_mesh[n]=NULL;
	m_Disc_Ind=NULL;
	m_Disc_epsR=NULL;
	m_Disc_kappa=NULL;
	m_Disc_mueR=NULL;
	m_Disc_sigma=NULL;
	m_Disc_Density=NULL;

	m_Scale=1;
	m_Transform=NULL;

	CSPropMaterial::Init();
}

bool CSPropDiscMaterial::Write2XML(TiXmlNode& root, bool parameterised, bool sparse)
{
	if (CSPropMaterial::Write2XML(root,parameterised,sparse) == false) return false;
	TiXmlElement* prop=root.ToElement();
	if (prop==NULL) return false;

	TiXmlElement filename("DiscFile");
	filename.SetAttribute("Type",m_FileType);
	filename.SetAttribute("File",m_Filename.c_str());

	filename.SetAttribute("Scale",m_Scale);

	if (m_Transform)
		m_Transform->Write2XML(prop);

	prop->InsertEndChild(filename);

	return true;
}

bool CSPropDiscMaterial::ReadFromXML(TiXmlNode &root)
{
	if (CSPropMaterial::ReadFromXML(root)==false) return false;
	TiXmlElement* prop=root.ToElement();

	if (prop==NULL) return false;

	m_FileType = 0;
	prop->QueryIntAttribute("Type",&m_FileType);
	const char* c_filename = prop->Attribute("File");

	delete m_Transform;
	m_Transform = CSTransform::New(prop, clParaSet);

	if (prop->QueryDoubleAttribute("Scale",&m_Scale)!=TIXML_SUCCESS)
		m_Scale=1;

	if (c_filename==NULL)
		return true;

	if ((m_FileType==0) && (c_filename!=NULL))
		return ReadHDF5(c_filename);
	else
		cerr << "CSPropDiscMaterial::ReadFromXML: Unknown file type or no filename given." << endl;

	return true;
}

float *CSPropDiscMaterial::ReadDataSet(string filename, string d_name, int &rank, unsigned int &size, bool debug)
{
	herr_t status;
	H5T_class_t class_id;
	size_t type_size;
	rank = -1;

	// open hdf5 file
	hid_t file_id = H5Fopen( filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT );
	if (file_id < 0)
	{
		if (debug)
			cerr << __func__ << ": Failed to open file, skipping..." << endl;
		H5Fclose(file_id);
		return NULL;
	}

	if (H5Lexists(file_id, d_name.c_str(), H5P_DEFAULT)<=0)
	{
		if (debug)
			cerr << __func__ << ": Warning, dataset: \"" << d_name << "\" not found... skipping" << endl;
		H5Fclose(file_id);
		return NULL;
	}

	status = H5LTget_dataset_ndims(file_id, d_name.c_str(), &rank);
	if (status < 0)
	{
		if (debug)
			cerr << __func__ << ": Warning, failed to read dimension for dataset: \"" << d_name << "\" skipping..." << endl;
		H5Fclose(file_id);
		return NULL;
	}

	hsize_t dims[rank];
	status = H5LTget_dataset_info( file_id, d_name.c_str(), dims, &class_id, &type_size);
	if (status < 0)
	{
		if (debug)
			cerr << __func__ << ": Warning, failed to read dataset info: \"" << d_name << "\" skipping..." << endl;
		H5Fclose(file_id);
		return NULL;
	}

	size = 1;
	for (int n=0;n<rank;++n)
		size*=dims[n];

	float* data = new float[size];
	status = H5LTread_dataset_float( file_id, d_name.c_str(), data );
	if (status < 0)
	{
		if (debug)
			cerr << __func__ << ": Warning, failed to read dataset: \"" << d_name << "\" skipping..." << endl;
		delete[] data;
		H5Fclose(file_id);
		return NULL;
	}

	H5Fclose(file_id);
	return data;
}

bool CSPropDiscMaterial::ReadHDF5( string filename )
{
	cout << __func__ << ": Reading \"" << filename << "\"" << endl;

	// open hdf5 file
	hid_t file_id = H5Fopen( filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT );
	if (file_id < 0)
	{
		cerr << __func__ << ": Error, failed to open file, abort..." << endl;
		return false;
	}

	double ver;
	herr_t status = H5LTget_attribute_double(file_id, "/", "Version", &ver);
	if (status < 0)
		ver = 1.0;

	if (ver<2.0)
	{
		cerr << __func__ << ": Error, older file versions are no longer supported, abort..." << endl;
		H5Fclose(file_id);
		return false;
	}

	int db_size;
	status = H5LTget_attribute_int(file_id, "/DiscData", "DB_Size", &db_size);
	if (status<0)
	{
		cerr << __func__ << ": Error, can't read database size, abort..." << endl;
		H5Fclose(file_id);
		return false;
	}

	m_DB_size = db_size;

	if (H5Lexists(file_id, "/DiscData", H5P_DEFAULT)<=0)
	{
		cerr << __func__ << ": Error, can't read database, abort..." << endl;
		H5Fclose(file_id);
		return false;
	}

	hid_t dataset = H5Dopen(file_id, "/DiscData", H5P_DEFAULT);
	if (dataset<0)
	{
		cerr << __func__ << ": Error, can't open database" << endl;
		H5Fclose(file_id);
		return 0;
	}

	// read mesh
	unsigned int size;
	int rank;
	unsigned int numCells = 1;
	string names[] = {"/mesh/x","/mesh/y","/mesh/z"};
	for (int n=0; n<3; ++n)
	{
		m_mesh[n] = ReadDataSet(filename, names[n], rank, size);
		if ((m_mesh[n]==NULL) || (rank!=1))
		{
			cerr << __func__ << ": Error, failed to read mesh, abort..." << endl;
			H5Fclose(file_id);
			return false;
		}
		m_Size[n]=size;
		numCells*=m_Size[n];
	}

	// read database
	if (H5LTfind_attribute(dataset, "epsR")==1)
	{
		m_Disc_epsR = new float[db_size];
		status = H5LTget_attribute_float(file_id, "/DiscData", "epsR", m_Disc_epsR);
	}
	else
	{
		cerr << __func__ << ": No \"/DiscData/epsR\" found, skipping..." << endl;
		m_Disc_epsR=NULL;
	}

	delete[] m_Disc_kappa;
	if (H5LTfind_attribute(dataset, "kappa")==1)
	{
		m_Disc_kappa = new float[db_size];
		status = H5LTget_attribute_float(file_id, "/DiscData", "kappa", m_Disc_kappa);
	}
	else
	{
		cerr << __func__ << ": No \"/DiscData/kappa\" found, skipping..." << endl;
		m_Disc_kappa=NULL;
	}

	delete[] m_Disc_mueR;
	if (H5LTfind_attribute(dataset, "mueR")==1)
	{
		m_Disc_mueR = new float[db_size];
		status = H5LTget_attribute_float(file_id, "/DiscData", "mueR", m_Disc_mueR);
	}
	else
	{
		cerr << __func__ << ": No \"/DiscData/mueR\" found, skipping..." << endl;
		m_Disc_mueR=NULL;
	}

	delete[] m_Disc_sigma;
	if (H5LTfind_attribute(dataset, "sigma")==1)
	{
		m_Disc_sigma = new float[db_size];
		status = H5LTget_attribute_float(file_id, "/DiscData", "sigma", m_Disc_sigma);
	}
	else
	{
		cerr << __func__ << ": No \"/DiscData/sigma\" found, skipping..." << endl;
		m_Disc_sigma=NULL;
	}

	delete[] m_Disc_Density;
	if (H5LTfind_attribute(dataset, "density")==1)
	{
		m_Disc_Density = new float[db_size];
		status = H5LTget_attribute_float(file_id, "/DiscData", "density", m_Disc_Density);
	}
	else
	{
		cerr << __func__ << ": no \"/DiscData/density\" found, skipping..." << endl;
		m_Disc_Density=NULL;
	}

	delete[] m_Disc_Ind;
	m_Disc_Ind = new int[numCells];
	status = H5LTread_dataset_int(file_id, "/DiscData", m_Disc_Ind);
	if (status<0)
	{
		cerr << __func__ << ": Error, can't read database indizies, abort..." << endl;
		delete[] m_Disc_Ind;
		H5Fclose(file_id);
		return false;
	}

	H5Fclose(file_id);
	return true;
}