/*!
 * \file geometry_structure.cpp
 * \brief Main subroutines for creating the primal grid and multigrid structure.
 * \author F. Palacios, T. Economon
 * \version 5.0.0 "Raven"
 *
 * SU2 Original Developers: Dr. Francisco D. Palacios.
 *                          Dr. Thomas D. Economon.
 *
 * SU2 Developers: Prof. Juan J. Alonso's group at Stanford University.
 *                 Prof. Piero Colonna's group at Delft University of Technology.
 *                 Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *                 Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *                 Prof. Rafael Palacios' group at Imperial College London.
 *                 Prof. Edwin van der Weide's group at the University of Twente.
 *                 Prof. Vincent Terrapon's group at the University of Liege.
 *
 * Copyright (C) 2012-2017 SU2, the open-source CFD code.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/geometry_structure.hpp"
#include "../include/adt_structure.hpp"
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>
#include "/home/kevin/Documents/Codelite_WENO/SU2_Github_Develop/SU2-develop/externals/lapack-3.7.1/LAPACKE/include/lapacke.h" 
#include <vector>
#include <algorithm>
#include <set>
/*--- Epsilon definition ---*/

#define EPSILON 0.000001
#define WENO3_K 5
#define WENO5_K 14
#define WENO3_J 10
#define WENO5_J 28
#define LDA WENO3_J
#define LDU WENO3_J
#define LDVT WENO3_K

//26/10/2017  debugging the LAPACKE function SVD to see if being read in and stored correctly
// the following are temporary containers for debugging only
#define TESTM 6
#define TESTN 5
#define TESTLDA TESTN
#define TESTLDU TESTM
#define TESTLDVT TESTN

/*--- Cross product ---*/

#define CROSS(dest,v1,v2) \
(dest)[0] = (v1)[1]*(v2)[2] - (v1)[2]*(v2)[1];	\
(dest)[1] = (v1)[2]*(v2)[0] - (v1)[0]*(v2)[2];	\
(dest)[2] = (v1)[0]*(v2)[1] - (v1)[1]*(v2)[0];

/*--- Cross product ---*/

#define DOT(v1,v2) ((v1)[0]*(v2)[0] + (v1)[1]*(v2)[1] + (v1)[2]*(v2)[2]);

/*--- a = b - c ---*/

#define SUB(dest,v1,v2) \
(dest)[0] = (v1)[0] - (v2)[0];	\
(dest)[1] = (v1)[1] - (v2)[1];	\
(dest)[2] = (v1)[2] - (v2)[2];

//extern void LAPACKE_dgesvd(int matrix_layout, char jobu, char jobvt,
 //                          int m, int n, double* a,
  ///                         int lda, double* s, double* u, int ldu,
//                           double* vt, int ldvt, double* superb); 

CGeometry::CGeometry(void) {
  
  nEdge      = 0;
  nPoint     = 0;
  nPointNode = 0;
  nElem      = 0;
  
  nElem_Bound         = NULL;
  Tag_to_Marker       = NULL;
  elem                = NULL;
  face                = NULL;
  bound               = NULL;
  node                = NULL;
  edge                = NULL;
  vertex              = NULL;
  nVertex             = NULL;
  newBound            = NULL;
  nNewElem_Bound      = NULL;
  Marker_All_SendRecv = NULL;
  
  PeriodicPoint[MAX_NUMBER_PERIODIC][2].clear();
  PeriodicElem[MAX_NUMBER_PERIODIC].clear();

  XCoordList.clear();
  Xcoord_plane.clear();
  Ycoord_plane.clear();
  Zcoord_plane.clear();
  FaceArea_plane.clear();
  Plane_points.clear();
  
  /*--- Arrays for defining the linear partitioning ---*/
  
  starting_node = NULL;
  ending_node   = NULL;
  npoint_procs  = NULL;
  
}

CGeometry::~CGeometry(void) {
  
  unsigned long iElem, iElem_Bound, iEdge, iFace, iPoint, iVertex;
  unsigned short iMarker;
  
  if (elem != NULL) {
    for (iElem = 0; iElem < nElem; iElem++)
      if (elem[iElem] != NULL) delete elem[iElem];
    delete[] elem;
  }
  
  if (bound != NULL) {
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        if (bound[iMarker][iElem_Bound] != NULL) delete bound[iMarker][iElem_Bound];
      }
      if (bound[iMarker] != NULL) delete [] bound[iMarker];
    }
    delete [] bound;
  }
  
  if (face != NULL) {
    for (iFace = 0; iFace < nFace; iFace ++)
      if (face[iFace] != NULL) delete face[iFace];
    delete[] face;
  }
  
  if (node != NULL) {
    for (iPoint = 0; iPoint < nPointNode; iPoint ++)
      if (node[iPoint] != NULL) delete node[iPoint];
    delete[] node;
  }
  
  
  if (edge != NULL) {
    for (iEdge = 0; iEdge < nEdge; iEdge ++)
      if (edge[iEdge] != NULL) delete edge[iEdge];
    delete[] edge;
  }

  if (vertex != NULL) {
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        if (vertex[iMarker][iVertex] != NULL) delete vertex[iMarker][iVertex];
      }
      if (vertex[iMarker] != NULL) delete [] vertex[iMarker];
    }
    delete [] vertex;
  }
  
  if (newBound != NULL) {
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        if (newBound[iMarker][iElem_Bound] != NULL) delete [] newBound[iMarker][iElem_Bound];
      }
      delete[] newBound[iMarker];
    }
    delete[] newBound;
  }
  
  if (nElem_Bound         != NULL) delete [] nElem_Bound;
  if (nVertex             != NULL) delete [] nVertex;
  if (nNewElem_Bound      != NULL) delete [] nNewElem_Bound;
  if (Marker_All_SendRecv != NULL) delete [] Marker_All_SendRecv;
  if (Tag_to_Marker       != NULL) delete [] Tag_to_Marker;
  
  if (starting_node != NULL) delete [] starting_node;
  if (ending_node   != NULL) delete [] ending_node;
  if (npoint_procs  != NULL) delete [] npoint_procs;
  
}

su2double CGeometry::Point2Plane_Distance(su2double *Coord, su2double *iCoord, su2double *jCoord, su2double *kCoord) {
  su2double CrossProduct[3], iVector[3], jVector[3], distance, modulus;
  unsigned short iDim;
  
  for (iDim = 0; iDim < 3; iDim ++) {
    iVector[iDim] = jCoord[iDim] - iCoord[iDim];
    jVector[iDim] = kCoord[iDim] - iCoord[iDim];
  }
  
  CrossProduct[0] = iVector[1]*jVector[2] - iVector[2]*jVector[1];
  CrossProduct[1] = iVector[2]*jVector[0] - iVector[0]*jVector[2];
  CrossProduct[2] = iVector[0]*jVector[1] - iVector[1]*jVector[0];
  
  modulus = sqrt(CrossProduct[0]*CrossProduct[0]+CrossProduct[1]*CrossProduct[1]+CrossProduct[2]*CrossProduct[2]);
  
  distance = 0.0;
  for (iDim = 0; iDim < 3; iDim ++)
    distance += CrossProduct[iDim]*(Coord[iDim]-iCoord[iDim]);
  distance /= modulus;
  
  return distance;
  
}

long CGeometry::FindEdge(unsigned long first_point, unsigned long second_point) {
  unsigned long iPoint = 0;
  unsigned short iNode;
  for (iNode = 0; iNode < node[first_point]->GetnPoint(); iNode++) {
    iPoint = node[first_point]->GetPoint(iNode);
    if (iPoint == second_point) break;
  }
  
  if (iPoint == second_point) return node[first_point]->GetEdge(iNode);
  else {
    cout << "\n\n   !!! Error !!!\n" << endl;
    cout <<"Can't find the edge that connects "<< first_point <<" and "<< second_point <<"."<< endl;
    exit(EXIT_FAILURE);
    return -1;
  }
}

bool CGeometry::CheckEdge(unsigned long first_point, unsigned long second_point) {
  unsigned long iPoint = 0;
  unsigned short iNode;
  for (iNode = 0; iNode < node[first_point]->GetnPoint(); iNode++) {
    iPoint = node[first_point]->GetPoint(iNode);
    if (iPoint == second_point) break;
  }
  
  if (iPoint == second_point) return true;
  else return false;
  
}

void CGeometry::SetEdges(void) {
  unsigned long iPoint, jPoint;
  long iEdge;
  unsigned short jNode, iNode;
  long TestEdge = 0;
  
  nEdge = 0;
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    for (iNode = 0; iNode < node[iPoint]->GetnPoint(); iNode++) {
      jPoint = node[iPoint]->GetPoint(iNode);
      for (jNode = 0; jNode < node[jPoint]->GetnPoint(); jNode++)
        if (node[jPoint]->GetPoint(jNode) == iPoint) {
          TestEdge = node[jPoint]->GetEdge(jNode);
          break;
        }
      if (TestEdge == -1) {
        node[iPoint]->SetEdge(nEdge, iNode);
        node[jPoint]->SetEdge(nEdge, jNode);
        nEdge++;
      }
    }
  
  edge = new CEdge*[nEdge];
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    for (iNode = 0; iNode < node[iPoint]->GetnPoint(); iNode++) {
      jPoint = node[iPoint]->GetPoint(iNode);
      iEdge = FindEdge(iPoint, jPoint);
      if (iPoint < jPoint) edge[iEdge] = new CEdge(iPoint, jPoint, nDim);
    }
}

void CGeometry::SetFaces(void) {
  //	unsigned long iPoint, jPoint, iFace;
  //	unsigned short jNode, iNode;
  //	long TestFace = 0;
  //
  //	nFace = 0;
  //	for (iPoint = 0; iPoint < nPoint; iPoint++)
  //		for (iNode = 0; iNode < node[iPoint]->GetnPoint(); iNode++) {
  //			jPoint = node[iPoint]->GetPoint(iNode);
  //			for (jNode = 0; jNode < node[jPoint]->GetnPoint(); jNode++)
  //				if (node[jPoint]->GetPoint(jNode) == iPoint) {
  //					TestFace = node[jPoint]->GetFace(jNode);
  //					break;
  //				}
  //			if (TestFace == -1) {
  //				node[iPoint]->SetFace(nFace, iNode);
  //				node[jPoint]->SetFace(nFace, jNode);
  //				nFace++;
  //			}
  //		}
  //
  //	face = new CFace*[nFace];
  //
  //	for (iPoint = 0; iPoint < nPoint; iPoint++)
  //		for (iNode = 0; iNode < node[iPoint]->GetnPoint(); iNode++) {
  //			jPoint = node[iPoint]->GetPoint(iNode);
  //			iFace = FindFace(iPoint, jPoint);
  //			if (iPoint < jPoint) face[iFace] = new CFace(iPoint, jPoint, nDim);
  //		}
}

void CGeometry::TestGeometry(void) {
  
  ofstream para_file;
  
  para_file.open("test_geometry.dat", ios::out);
  
  su2double *Normal = new su2double[nDim];
  
  for (unsigned long iEdge = 0; iEdge < nEdge; iEdge++) {
    para_file << "Edge index: " << iEdge << endl;
    para_file << "   Point index: " << edge[iEdge]->GetNode(0) << "\t" << edge[iEdge]->GetNode(1) << endl;
    edge[iEdge]->GetNormal(Normal);
    para_file << "      Face normal : ";
    for (unsigned short iDim = 0; iDim < nDim; iDim++)
      para_file << Normal[iDim] << "\t";
    para_file << endl;
  }
  
  para_file << endl;
  para_file << endl;
  para_file << endl;
  para_file << endl;
  
  for (unsigned short iMarker =0; iMarker < nMarker; iMarker++) {
    para_file << "Marker index: " << iMarker << endl;
    for (unsigned long iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      para_file << "   Vertex index: " << iVertex << endl;
      para_file << "      Point index: " << vertex[iMarker][iVertex]->GetNode() << endl;
      para_file << "      Point coordinates : ";
      for (unsigned short iDim = 0; iDim < nDim; iDim++) {
        para_file << node[vertex[iMarker][iVertex]->GetNode()]->GetCoord(iDim) << "\t";}
      para_file << endl;
      vertex[iMarker][iVertex]->GetNormal(Normal);
      para_file << "         Face normal : ";
      for (unsigned short iDim = 0; iDim < nDim; iDim++)
        para_file << Normal[iDim] << "\t";
      para_file << endl;
    }
  }
  
  delete [] Normal;
  
}

void CGeometry::SetSpline(vector<su2double> &x, vector<su2double> &y, unsigned long n, su2double yp1, su2double ypn, vector<su2double> &y2) {
  unsigned long i, k;
  su2double p, qn, sig, un, *u;
  
  u = new su2double [n];
  
  if (yp1 > 0.99e30)			// The lower boundary condition is set either to be "nat
    y2[0]=u[0]=0.0;			  // -ural"
  else {									// or else to have a specified first derivative.
    y2[0] = -0.5;
    u[0]=(3.0/(x[1]-x[0]))*((y[1]-y[0])/(x[1]-x[0])-yp1);
  }
  
  for (i=2; i<=n-1; i++) {									//  This is the decomposition loop of the tridiagonal al-
    sig=(x[i-1]-x[i-2])/(x[i]-x[i-2]);		//	gorithm. y2 and u are used for tem-
    p=sig*y2[i-2]+2.0;										//	porary storage of the decomposed
    y2[i-1]=(sig-1.0)/p;										//	factors.
    
    su2double a1 = (y[i]-y[i-1])/(x[i]-x[i-1]); if (x[i] == x[i-1]) a1 = 1.0;
    su2double a2 = (y[i-1]-y[i-2])/(x[i-1]-x[i-2]); if (x[i-1] == x[i-2]) a2 = 1.0;
    u[i-1]= a1 - a2;
    u[i-1]=(6.0*u[i-1]/(x[i]-x[i-2])-sig*u[i-2])/p;
    
  }
  
  if (ypn > 0.99e30)						// The upper boundary condition is set either to be
    qn=un=0.0;									// "natural"
  else {												// or else to have a specified first derivative.
    qn=0.5;
    un=(3.0/(x[n-1]-x[n-2]))*(ypn-(y[n-1]-y[n-2])/(x[n-1]-x[n-2]));
  }
  y2[n-1]=(un-qn*u[n-2])/(qn*y2[n-2]+1.0);
  for (k=n-1; k>=1; k--)					// This is the backsubstitution loop of the tridiagonal
    y2[k-1]=y2[k-1]*y2[k]+u[k-1];	  // algorithm.
  
  delete[] u;
  
}

su2double CGeometry::GetSpline(vector<su2double>&xa, vector<su2double>&ya, vector<su2double>&y2a, unsigned long n, su2double x) {
  unsigned long klo, khi, k;
  su2double h, b, a, y;
  
  if (x < xa[0]) x = xa[0];       // Clip max and min values
  if (x > xa[n-1]) x = xa[n-1];
  
  klo = 1;										// We will find the right place in the table by means of
  khi = n;										// bisection. This is optimal if sequential calls to this
  while (khi-klo > 1) {			// routine are at random values of x. If sequential calls
    k = (khi+klo) >> 1;				// are in order, and closely spaced, one would do better
    if (xa[k-1] > x) khi = k;		// to store previous values of klo and khi and test if
    else klo=k;							// they remain appropriate on the next call.
  }								// klo and khi now bracket the input value of x
  h = xa[khi-1] - xa[klo-1];
  if (h == 0.0) h = EPS; // cout << "Bad xa input to routine splint" << endl;	// The xa???s must be distinct.
  a = (xa[khi-1]-x)/h;
  b = (x-xa[klo-1])/h;				// Cubic spline polynomial is now evaluated.
  y = a*ya[klo-1]+b*ya[khi-1]+((a*a*a-a)*y2a[klo-1]+(b*b*b-b)*y2a[khi-1])*(h*h)/6.0;
  
  return y;
}

bool CGeometry::SegmentIntersectsPlane(su2double *Segment_P0, su2double *Segment_P1, su2double Variable_P0, su2double Variable_P1,
                                                           su2double *Plane_P0, su2double *Plane_Normal, su2double *Intersection, su2double &Variable_Interp) {
  su2double u[3], v[3], Denominator, Numerator, Aux, ModU;
  unsigned short iDim;
  
  for (iDim = 0; iDim < 3; iDim++) {
    u[iDim] = Segment_P1[iDim] - Segment_P0[iDim];
    v[iDim] = Plane_P0[iDim] - Segment_P0[iDim];
  }
  
  ModU = sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);
  
  Numerator = Plane_Normal[0]*v[0] + Plane_Normal[1]*v[1] + Plane_Normal[2]*v[2];
  Denominator = Plane_Normal[0]*u[0] + Plane_Normal[1]*u[1] + Plane_Normal[2]*u[2];
  
  if (fabs(Denominator) <= 0.0) return (false); // No intersection.
  
  Aux = Numerator / Denominator;
  
  if (Aux < 0.0 || Aux > 1.0) return (false); // No intersection.
  
  for (iDim = 0; iDim < 3; iDim++)
    Intersection[iDim] = Segment_P0[iDim] + Aux * u[iDim];
  
  
  /*--- Check that the intersection is in the segment ---*/
  
  for (iDim = 0; iDim < 3; iDim++) {
    u[iDim] = Segment_P0[iDim] - Intersection[iDim];
    v[iDim] = Segment_P1[iDim] - Intersection[iDim];
  }
  
  Variable_Interp = Variable_P0 + (Variable_P1 - Variable_P0)*sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2])/ModU;
  
  Denominator = Plane_Normal[0]*u[0] + Plane_Normal[1]*u[1] + Plane_Normal[2]*u[2];
  Numerator = Plane_Normal[0]*v[0] + Plane_Normal[1]*v[1] + Plane_Normal[2]*v[2];
  
  Aux = Numerator * Denominator;
  
  if (Aux > 0.0) return (false); // Intersection outside the segment.
  
  return (true);
  
}

bool CGeometry::RayIntersectsTriangle(su2double orig[3], su2double dir[3],
                                      su2double vert0[3], su2double vert1[3], su2double vert2[3],
                                      su2double *intersect) {
  
  su2double edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
  su2double det, inv_det, t, u, v;
  
  /*--- Find vectors for two edges sharing vert0 ---*/
  
  SUB(edge1, vert1, vert0);
  SUB(edge2, vert2, vert0);
  
  /*--- Begin calculating determinant - also used to calculate U parameter ---*/
  
  CROSS(pvec, dir, edge2);
  
  /*--- If determinant is near zero, ray lies in plane of triangle ---*/
  
  det = DOT(edge1, pvec);
  
  
  if (det > -EPSILON && det < EPSILON) return(false);
  
  inv_det = 1.0 / det;
  
  /*--- Calculate distance from vert0 to ray origin ---*/
  
  SUB(tvec, orig, vert0);
  
  /*--- Calculate U parameter and test bounds ---*/
  
  u = inv_det * DOT(tvec, pvec);
  
  if (u < 0.0 || u > 1.0) return(false);
  
  /*--- prepare to test V parameter ---*/
  
  CROSS(qvec, tvec, edge1);
  
  /*--- Calculate V parameter and test bounds ---*/
  
  v = inv_det * DOT(dir, qvec);
  
  if (v < 0.0 || u + v > 1.0) return(false);
  
  /*--- Calculate t, ray intersects triangle ---*/
  
  t = inv_det * DOT(edge2, qvec);
  
  /*--- Compute the intersection point in cartesian coordinates ---*/
  
  intersect[0] = orig[0] + (t * dir[0]);
  intersect[1] = orig[1] + (t * dir[1]);
  intersect[2] = orig[2] + (t * dir[2]);

  return (true);
  
}

bool CGeometry::SegmentIntersectsLine(su2double point0[2], su2double point1[2], su2double vert0[2], su2double vert1[2]) {

  su2double det, diff0_A, diff0_B, diff1_A, diff1_B, intersect[2];

  diff0_A = point0[0] - point1[0];
  diff1_A = point0[1] - point1[1];

  diff0_B = vert0[0] - vert1[0];
  diff1_B = vert0[1] - vert1[1];

  det = (diff0_A)*(diff1_B) - (diff1_A)*(diff0_B);

  if (det == 0) return false;

  /*--- Compute point of intersection ---*/

  intersect[0] = ((point0[0]*point1[1] - point0[1]*point1[0])*diff0_B
                -(vert0[0]* vert1[1]  - vert0[1]* vert1[0])*diff0_A)/det;

  intersect[1] =  ((point0[0]*point1[1] - point0[1]*point1[0])*diff1_B
                  -(vert0[0]* vert1[1]  - vert0[1]* vert1[0])*diff1_A)/det;


  /*--- Check that the point is between the two surface points ---*/

  su2double dist0, dist1, length;

  dist0 = (intersect[0] - point0[0])*(intersect[0] - point0[0])
         +(intersect[1] - point0[1])*(intersect[1] - point0[1]);

  dist1 = (intersect[0] - point1[0])*(intersect[0] - point1[0])
         +(intersect[1] - point1[1])*(intersect[1] - point1[1]);

  length = diff0_A*diff0_A
          +diff1_A*diff1_A;

  if ( (dist0 > length) || (dist1 > length) ) {
    return false;
  }

  return true;
}

bool CGeometry::SegmentIntersectsTriangle(su2double point0[3], su2double point1[3],
                                          su2double vert0[3], su2double vert1[3], su2double vert2[3]) {
  
  su2double dir[3], intersect[3], u[3], v[3], edge1[3], edge2[3], Plane_Normal[3], Denominator, Numerator, Aux;
  
  SUB(dir, point1, point0);
  
  if (RayIntersectsTriangle(point0, dir, vert0, vert1, vert2, intersect)) {
    
    /*--- Check that the intersection is in the segment ---*/
    
    SUB(u, point0, intersect);
    SUB(v, point1, intersect);
    
    SUB(edge1, vert1, vert0);
    SUB(edge2, vert2, vert0);
    CROSS(Plane_Normal, edge1, edge2);
    
    Denominator = DOT(Plane_Normal, u);
    Numerator = DOT(Plane_Normal, v);
    
    Aux = Numerator * Denominator;
    
    /*--- Intersection outside the segment ---*/
    
    if (Aux > 0.0) return (false);
    
  }
  else {
    
    /*--- No intersection with the ray ---*/
    
    return (false);
    
  }
  
  /*--- Intersection inside the segment ---*/

  return (true);
  
}

void CGeometry::ComputeAirfoil_Section(su2double *Plane_P0, su2double *Plane_Normal,
                                       su2double MinXCoord, su2double MaxXCoord, su2double *FlowVariable,
                                       vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil,
                                       vector<su2double> &Zcoord_Airfoil, vector<su2double> &Variable_Airfoil,
                                       bool original_surface, CConfig *config) {
  
  unsigned short iMarker, iNode, jNode, iDim, Index = 0;
  bool intersect;
  long MinDist_Point;
  unsigned long iPoint, jPoint, iElem, Trailing_Point, Airfoil_Point, iVertex, PointIndex, jVertex;
  su2double Segment_P0[3] = {0.0, 0.0, 0.0}, Segment_P1[3] = {0.0, 0.0, 0.0}, Variable_P0 = 0.0, Variable_P1 = 0.0, Intersection[3] = {0.0, 0.0, 0.0}, Trailing_Coord,
  *VarCoord = NULL, Variable_Interp;
  passivedouble MinDist_Value, Dist_Value, Segment[3] = {0.0, 0.0, 0.0}, Tolerance = 1E-10;
  vector<su2double> Xcoord_Index0, Ycoord_Index0, Zcoord_Index0, Variable_Index0, Xcoord_Index1, Ycoord_Index1, Zcoord_Index1, Variable_Index1;
  vector<unsigned short> Conection_Index0, Conection_Index1;
  vector<unsigned long> Duplicate;
  vector<unsigned long>::iterator it;
  int rank = MASTER_NODE;
  su2double **Coord_Variation = NULL;
  
#ifdef HAVE_MPI
  unsigned long nLocalVertex, MaxLocalVertex, *Buffer_Send_nVertex, *Buffer_Receive_nVertex, nBuffer;
  int nProcessor, iProcessor;
  
  su2double *Buffer_Send_Coord, *Buffer_Receive_Coord;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  Xcoord_Airfoil.clear();
  Ycoord_Airfoil.clear();
  Zcoord_Airfoil.clear();
  Variable_Airfoil.clear();
  
  /*--- Set the right plane in 2D (note the change in Y-Z plane) ---*/
  
  if (nDim == 2) {
    Plane_P0[0] = 0.0;      Plane_P0[1] = 0.0;      Plane_P0[2] = 0.0;
    Plane_Normal[0] = 0.0;  Plane_Normal[1] = 1.0;  Plane_Normal[2] = 0.0;
  }
  
  /*--- the grid variation is stored using a vertices information,
   we should go from vertex to points ---*/
  
  if (original_surface == false) {
    
    Coord_Variation = new su2double *[nPoint];
    for (iPoint = 0; iPoint < nPoint; iPoint++)
    Coord_Variation[iPoint] = new su2double [nDim];
    
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
      if (config->GetMarker_All_GeoEval(iMarker) == YES) {
        for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
          VarCoord = vertex[iMarker][iVertex]->GetVarCoord();
          iPoint = vertex[iMarker][iVertex]->GetNode();
          for (iDim = 0; iDim < nDim; iDim++)
          Coord_Variation[iPoint][iDim] = VarCoord[iDim];
        }
      }
    }
    
  }
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (config->GetMarker_All_GeoEval(iMarker) == YES) {
      
      for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++) {
        PointIndex=0;
        
        for (iNode = 0; iNode < bound[iMarker][iElem]->GetnNodes(); iNode++) {
          iPoint = bound[iMarker][iElem]->GetNode(iNode);
          
          for (jNode = 0; jNode < bound[iMarker][iElem]->GetnNodes(); jNode++) {
            jPoint = bound[iMarker][iElem]->GetNode(jNode);
            
            if ((jPoint > iPoint) && ((node[iPoint]->GetCoord(0) > MinXCoord) && (node[iPoint]->GetCoord(0) < MaxXCoord))) {
              
              Segment_P0[0] = 0.0;  Segment_P0[1] = 0.0;  Segment_P0[2] = 0.0;  Variable_P0 = 0.0;
              Segment_P1[0] = 0.0;  Segment_P1[1] = 0.0;  Segment_P1[2] = 0.0;  Variable_P1 = 0.0;
              
              for (iDim = 0; iDim < nDim; iDim++) {
                if (original_surface == true) {
                  Segment_P0[iDim] = node[iPoint]->GetCoord(iDim);
                  Segment_P1[iDim] = node[jPoint]->GetCoord(iDim);
                }
                else {
                  Segment_P0[iDim] = node[iPoint]->GetCoord(iDim) + Coord_Variation[iPoint][iDim];
                  Segment_P1[iDim] = node[jPoint]->GetCoord(iDim) + Coord_Variation[jPoint][iDim];
                }
              }
              
              if (FlowVariable != NULL) {
                Variable_P0 = FlowVariable[iPoint];
                Variable_P1 = FlowVariable[jPoint];
              }
              
              /*--- In 2D add the points directly (note the change between Y and Z coordinate) ---*/
              
              if (nDim == 2) {
                
                Xcoord_Index0.push_back(Segment_P0[0]);    Xcoord_Index1.push_back(Segment_P1[0]);
                Ycoord_Index0.push_back(Segment_P0[2]);    Ycoord_Index1.push_back(Segment_P1[2]);
                Zcoord_Index0.push_back(Segment_P0[1]);    Zcoord_Index1.push_back(Segment_P1[1]);
                Variable_Index0.push_back(Variable_P0);    Variable_Index1.push_back(Variable_P1);
                PointIndex++;
                
              }
              /*--- In 3D compute the intersection ---*/
              
              else if (nDim == 3) {
                
                intersect = SegmentIntersectsPlane(Segment_P0, Segment_P1, Variable_P0, Variable_P1, Plane_P0, Plane_Normal, Intersection, Variable_Interp);
                
                if (intersect == true) {
                  
                  if (PointIndex == 0) {
                    Xcoord_Index0.push_back(Intersection[0]);
                    Ycoord_Index0.push_back(Intersection[1]);
                    Zcoord_Index0.push_back(Intersection[2]);
                    Variable_Index0.push_back(Variable_Interp);
                  }
                  if (PointIndex == 1) {
                    Xcoord_Index1.push_back(Intersection[0]);
                    Ycoord_Index1.push_back(Intersection[1]);
                    Zcoord_Index1.push_back(Intersection[2]);
                    Variable_Index1.push_back(Variable_Interp);
                  }
                  PointIndex++;
                }
              }
              
            }
            
          }
        }
        
        
        
      }
    }
  }
  
  if (original_surface == false) {
    
    for (iPoint = 0; iPoint < nPoint; iPoint++)
    delete [] Coord_Variation[iPoint];
    delete [] Coord_Variation;
    
  }
  
#ifdef HAVE_MPI
  
  /*--- Copy the coordinates of all the points in the plane to the master node ---*/
  
  nLocalVertex = 0, MaxLocalVertex = 0;
  MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
  
  Buffer_Send_nVertex = new unsigned long [1];
  Buffer_Receive_nVertex = new unsigned long [nProcessor];
  
  nLocalVertex = Xcoord_Index0.size();
  
  Buffer_Send_nVertex[0] = nLocalVertex;
  
  SU2_MPI::Allreduce(&nLocalVertex, &MaxLocalVertex, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
  
  Buffer_Send_Coord = new su2double [MaxLocalVertex*8];
  Buffer_Receive_Coord = new su2double [nProcessor*MaxLocalVertex*8];
  nBuffer = MaxLocalVertex*8;
  
  for (iVertex = 0; iVertex < nLocalVertex; iVertex++) {
    Buffer_Send_Coord[iVertex*8 + 0] = Xcoord_Index0[iVertex];
    Buffer_Send_Coord[iVertex*8 + 1] = Ycoord_Index0[iVertex];
    Buffer_Send_Coord[iVertex*8 + 2] = Zcoord_Index0[iVertex];
    Buffer_Send_Coord[iVertex*8 + 3] = Variable_Index0[iVertex];
    Buffer_Send_Coord[iVertex*8 + 4] = Xcoord_Index1[iVertex];
    Buffer_Send_Coord[iVertex*8 + 5] = Ycoord_Index1[iVertex];
    Buffer_Send_Coord[iVertex*8 + 6] = Zcoord_Index1[iVertex];
    Buffer_Send_Coord[iVertex*8 + 7] = Variable_Index1[iVertex];
  }
  
  SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer, MPI_DOUBLE, MPI_COMM_WORLD);
  
  /*--- Clean the vectors before adding the new vertices only to the master node ---*/
  
  Xcoord_Index0.clear();     Xcoord_Index1.clear();
  Ycoord_Index0.clear();     Ycoord_Index1.clear();
  Zcoord_Index0.clear();     Zcoord_Index1.clear();
  Variable_Index0.clear();   Variable_Index1.clear();
  
  /*--- Copy the boundary to the master node vectors ---*/
  
  if (rank == MASTER_NODE) {
    for (iProcessor = 0; iProcessor < nProcessor; iProcessor++) {
      for (iVertex = 0; iVertex < Buffer_Receive_nVertex[iProcessor]; iVertex++) {
        Xcoord_Index0.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 0] );
        Ycoord_Index0.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 1] );
        Zcoord_Index0.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 2] );
        Variable_Index0.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 3] );
        Xcoord_Index1.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 4] );
        Ycoord_Index1.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 5] );
        Zcoord_Index1.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 6] );
        Variable_Index1.push_back( Buffer_Receive_Coord[ iProcessor*MaxLocalVertex*8 + iVertex*8 + 7] );
      }
    }
  }
  
  delete[] Buffer_Send_Coord;   delete[] Buffer_Receive_Coord;
  delete[] Buffer_Send_nVertex; delete[] Buffer_Receive_nVertex;
  
#endif
  
  
  
  if ((rank == MASTER_NODE) && (Xcoord_Index0.size() != 0)) {
    
    /*--- Remove edges with zero length ---*/
    
    bool Remove;
    
    do {
      
      Remove = false;
      
      for (iVertex = 0; iVertex < Xcoord_Index0.size(); iVertex++) {
        
        Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index1[iVertex]);
        Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index1[iVertex]);
        Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index1[iVertex]);
        
        Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
        
        if (Dist_Value <= Tolerance) {
          
          /*--- Remove edge with repeated points ---*/
          
          Xcoord_Index0.erase (Xcoord_Index0.begin() + iVertex);
          Ycoord_Index0.erase (Ycoord_Index0.begin() + iVertex);
          Zcoord_Index0.erase (Zcoord_Index0.begin() + iVertex);
          Variable_Index0.erase (Variable_Index0.begin() + iVertex);
          
          Xcoord_Index1.erase (Xcoord_Index1.begin() + iVertex);
          Ycoord_Index1.erase (Ycoord_Index1.begin() + iVertex);
          Zcoord_Index1.erase (Zcoord_Index1.begin() + iVertex);
          Variable_Index1.erase (Variable_Index1.begin() + iVertex);
          
          Remove = true; break;
          
        }
        
        if (Remove) break;
        
      }
      
    } while (Remove == true);
    
    
    /*--- Remove repeated edges computing distance ---*/
    
    do {
      
      Remove = false;
      
      for (iVertex = 0; iVertex < Xcoord_Index0.size()-1; iVertex++) {
        for (jVertex = iVertex+1; jVertex < Xcoord_Index0.size(); jVertex++) {
          
          /*--- First check ---*/
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index0[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index0[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index0[jVertex]);
          
          /*--- Compute the distance to each point ---*/
          
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          
          /*--- Check the second point ---*/
          
          if (Dist_Value <= Tolerance) {
            
            Segment[0] = SU2_TYPE::GetValue(Xcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index1[jVertex]);
            Segment[1] = SU2_TYPE::GetValue(Ycoord_Index1[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index1[jVertex]);
            Segment[2] = SU2_TYPE::GetValue(Zcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index1[jVertex]);
            
            /*--- Compute the distance to each point ---*/
            
            Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
            
            if (Dist_Value <= Tolerance) {
              
              /*--- Remove repeated point ---*/
              
              Xcoord_Index0.erase (Xcoord_Index0.begin() + jVertex);
              Ycoord_Index0.erase (Ycoord_Index0.begin() + jVertex);
              Zcoord_Index0.erase (Zcoord_Index0.begin() + jVertex);
              Variable_Index0.erase (Variable_Index0.begin() + jVertex);
              
              Xcoord_Index1.erase (Xcoord_Index1.begin() + jVertex);
              Ycoord_Index1.erase (Ycoord_Index1.begin() + jVertex);
              Zcoord_Index1.erase (Zcoord_Index1.begin() + jVertex);
              Variable_Index1.erase (Variable_Index1.begin() + jVertex);
              
              Remove = true; break;
              
            }
            
          }
          
          /*--- Second check ---*/
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index1[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index1[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index1[jVertex]);
          
          /*--- Compute the distance to each point ---*/
          
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          
          /*--- Check the second point ---*/
          
          if (Dist_Value <= Tolerance) {
            
            Segment[0] = SU2_TYPE::GetValue(Xcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index0[jVertex]);
            Segment[1] = SU2_TYPE::GetValue(Ycoord_Index1[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index0[jVertex]);
            Segment[2] = SU2_TYPE::GetValue(Zcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index0[jVertex]);
            
            /*--- Compute the distance to each point ---*/
            
            Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
            
            if (Dist_Value <= Tolerance) {
              
              /*--- Remove repeated point ---*/
              
              Xcoord_Index0.erase (Xcoord_Index0.begin() + jVertex);
              Ycoord_Index0.erase (Ycoord_Index0.begin() + jVertex);
              Zcoord_Index0.erase (Zcoord_Index0.begin() + jVertex);
              Variable_Index0.erase (Variable_Index0.begin() + jVertex);
              
              Xcoord_Index1.erase (Xcoord_Index1.begin() + jVertex);
              Ycoord_Index1.erase (Ycoord_Index1.begin() + jVertex);
              Zcoord_Index1.erase (Zcoord_Index1.begin() + jVertex);
              Variable_Index1.erase (Variable_Index1.begin() + jVertex);
              
              Remove = true; break;
            }
          }
          if (Remove) break;
        }
        if (Remove) break;
      }
      
    } while (Remove == true);
    
    if (Xcoord_Index0.size() != 1) {
      
      if (config->GetGeo_Description() == FUSELAGE) {
        
        /*--- Rotate from the Y-Z plane to the X-Z plane to reuse the rest of subroutines  ---*/
        
        su2double Angle = -0.5*PI_NUMBER;
        
        for (iVertex = 0; iVertex < Xcoord_Index0.size(); iVertex++) {
          
          su2double XCoord = Xcoord_Index0[iVertex]*cos(Angle) - Ycoord_Index0[iVertex]*sin(Angle);
          su2double YCoord = Ycoord_Index0[iVertex]*cos(Angle) + Xcoord_Index0[iVertex]*sin(Angle);
          su2double ZCoord = Zcoord_Index0[iVertex];
          
          Xcoord_Index0[iVertex] = XCoord;
          Ycoord_Index0[iVertex] = YCoord;
          Zcoord_Index0[iVertex] = ZCoord;
          
          XCoord = Xcoord_Index1[iVertex]*cos(Angle) - Ycoord_Index1[iVertex]*sin(Angle);
          YCoord = Ycoord_Index1[iVertex]*cos(Angle) + Xcoord_Index1[iVertex]*sin(Angle);
          ZCoord = Zcoord_Index1[iVertex];
          Xcoord_Index1[iVertex] = XCoord;
          Ycoord_Index1[iVertex] = YCoord;
          Zcoord_Index1[iVertex] = ZCoord;
          
        }
        
      }
      
      /*--- Identify the extreme of the curve and close it ---*/
      
      Conection_Index0.reserve(Xcoord_Index0.size()+1);
      Conection_Index1.reserve(Xcoord_Index0.size()+1);
      
      for (iVertex = 0; iVertex < Xcoord_Index0.size(); iVertex++) {
        Conection_Index0[iVertex] = 0;
        Conection_Index1[iVertex] = 0;
      }
      
      for (iVertex = 0; iVertex < Xcoord_Index0.size()-1; iVertex++) {
        for (jVertex = iVertex+1; jVertex < Xcoord_Index0.size(); jVertex++) {
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index0[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index0[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index0[jVertex]);
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= Tolerance) { Conection_Index0[iVertex]++; Conection_Index0[jVertex]++; }
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index1[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index1[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index1[jVertex]);
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= Tolerance) { Conection_Index0[iVertex]++; Conection_Index1[jVertex]++; }
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index0[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index1[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index0[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index0[jVertex]);
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= Tolerance) { Conection_Index1[iVertex]++; Conection_Index0[jVertex]++; }
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Xcoord_Index1[jVertex]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index1[iVertex]) - SU2_TYPE::GetValue(Ycoord_Index1[jVertex]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Zcoord_Index1[jVertex]);
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= Tolerance) { Conection_Index1[iVertex]++; Conection_Index1[jVertex]++; }
          
        }
      }
      
      /*--- Connect extremes of the curves ---*/
      
      vector<su2double> XcoordExtra, YcoordExtra, ZcoordExtra, VariableExtra;
      vector<bool> AddExtra;
      unsigned long VertexDonor;
      bool FoundVertex;
      
      for (iVertex = 0; iVertex < Xcoord_Index0.size(); iVertex++) {
        if (Conection_Index0[iVertex] == 0) {
          XcoordExtra.push_back(Xcoord_Index0[iVertex]);
          YcoordExtra.push_back(Ycoord_Index0[iVertex]);
          ZcoordExtra.push_back(Zcoord_Index0[iVertex]);
          VariableExtra.push_back(Variable_Index0[iVertex]);
          AddExtra.push_back(true);
        }
        if (Conection_Index1[iVertex] == 0) {
          XcoordExtra.push_back(Xcoord_Index1[iVertex]);
          YcoordExtra.push_back(Ycoord_Index1[iVertex]);
          ZcoordExtra.push_back(Zcoord_Index1[iVertex]);
          VariableExtra.push_back(Variable_Index1[iVertex]);
          AddExtra.push_back(true);
        }
      }
      
      /*---Compute min distance from the points ---*/
      
      if (XcoordExtra.size() > 1) {
        
        for (iVertex = 0; iVertex < XcoordExtra.size()-1; iVertex++) {
          
          su2double MinDist = 1E6;
          FoundVertex = false;
          VertexDonor = 0;
          
          for (jVertex = iVertex+1; jVertex <XcoordExtra.size(); jVertex++) {
            Dist_Value = sqrt(pow(SU2_TYPE::GetValue(XcoordExtra[iVertex])-SU2_TYPE::GetValue(XcoordExtra[jVertex]), 2.0));
            if ((Dist_Value < MinDist) && (AddExtra[iVertex]) && (AddExtra[jVertex])) {
              VertexDonor = jVertex;
              FoundVertex = true;
            }
          }
          
          if (FoundVertex) {
            
            /*--- Add first point of the new edge ---*/
            
            Xcoord_Index0.push_back (XcoordExtra[iVertex]);
            Ycoord_Index0.push_back (YcoordExtra[iVertex]);
            Zcoord_Index0.push_back (ZcoordExtra[iVertex]);
            Variable_Index0.push_back (VariableExtra[iVertex]);
            AddExtra[iVertex] = false;
            
            /*--- Add second (closest)  point of the new edge ---*/
            
            Xcoord_Index1.push_back (XcoordExtra[VertexDonor]);
            Ycoord_Index1.push_back (YcoordExtra[VertexDonor]);
            Zcoord_Index1.push_back (ZcoordExtra[VertexDonor]);
            Variable_Index1.push_back (VariableExtra[VertexDonor]);
            AddExtra[VertexDonor] = false;
            
          }
          
        }
        
      }
      
      else if (XcoordExtra.size() == 1) {
        cout <<"There cutting system has failed, there is an incomplete curve (not used)." << endl;
      }
      
      Trailing_Point = 0; Trailing_Coord = Xcoord_Index0[0];
      for (iVertex = 1; iVertex < Xcoord_Index0.size(); iVertex++) {
        if (Xcoord_Index0[iVertex] > Trailing_Coord) {
          Trailing_Point = iVertex; Trailing_Coord = Xcoord_Index0[iVertex];
        }
      }
      
      /*--- Add the trailing edge to the list and the contect points to the trailing edge and remove from the original list ---*/
      
      Xcoord_Airfoil.push_back(Xcoord_Index0[Trailing_Point]);
      Ycoord_Airfoil.push_back(Ycoord_Index0[Trailing_Point]);
      Zcoord_Airfoil.push_back(Zcoord_Index0[Trailing_Point]);
      Variable_Airfoil.push_back(Variable_Index0[Trailing_Point]);
      
      Xcoord_Airfoil.push_back(Xcoord_Index1[Trailing_Point]);
      Ycoord_Airfoil.push_back(Ycoord_Index1[Trailing_Point]);
      Zcoord_Airfoil.push_back(Zcoord_Index1[Trailing_Point]);
      Variable_Airfoil.push_back(Variable_Index1[Trailing_Point]);
      
      Xcoord_Index0.erase (Xcoord_Index0.begin() + Trailing_Point);
      Ycoord_Index0.erase (Ycoord_Index0.begin() + Trailing_Point);
      Zcoord_Index0.erase (Zcoord_Index0.begin() + Trailing_Point);
      Variable_Index0.erase (Variable_Index0.begin() + Trailing_Point);
      
      Xcoord_Index1.erase (Xcoord_Index1.begin() + Trailing_Point);
      Ycoord_Index1.erase (Ycoord_Index1.begin() + Trailing_Point);
      Zcoord_Index1.erase (Zcoord_Index1.begin() + Trailing_Point);
      Variable_Index1.erase (Variable_Index1.begin() + Trailing_Point);
      
      /*--- Algorithm for the rest of the points ---*/
      
      do {
        
        /*--- Last added point in the list ---*/
        
        Airfoil_Point = Xcoord_Airfoil.size() - 1;
        
        /*--- Find the closest point  ---*/
        
        MinDist_Value = 1E6;
        MinDist_Point = -1;
        
        for (iVertex = 0; iVertex < Xcoord_Index0.size(); iVertex++) {
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Xcoord_Airfoil[Airfoil_Point]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index0[iVertex]) - SU2_TYPE::GetValue(Ycoord_Airfoil[Airfoil_Point]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index0[iVertex]) - SU2_TYPE::GetValue(Zcoord_Airfoil[Airfoil_Point]);
          
          /*--- Compute the distance to each point ---*/
          
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= MinDist_Value) {
            MinDist_Point = iVertex;
            MinDist_Value = Dist_Value;
            Index = 0;
          }
          
          /*--- Same with the second index  ---*/
          
          Segment[0] = SU2_TYPE::GetValue(Xcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Xcoord_Airfoil[Airfoil_Point]);
          Segment[1] = SU2_TYPE::GetValue(Ycoord_Index1[iVertex]) - SU2_TYPE::GetValue(Ycoord_Airfoil[Airfoil_Point]);
          Segment[2] = SU2_TYPE::GetValue(Zcoord_Index1[iVertex]) - SU2_TYPE::GetValue(Zcoord_Airfoil[Airfoil_Point]);
          
          /*--- Compute the distance to each point ---*/
          
          Dist_Value = sqrt(pow(Segment[0], 2.0) + pow(Segment[1], 2.0) + pow(Segment[2], 2.0));
          if (Dist_Value <= MinDist_Value) {
            MinDist_Point = iVertex;
            MinDist_Value = Dist_Value;
            Index = 1;
          }
          
        }
        
        /*--- Add and remove the min distance to the list and the next point in the edge ---*/
        
        if (MinDist_Value < Tolerance) {
          
          if (Index == 0) {
            
            Xcoord_Airfoil.push_back(Xcoord_Index1[MinDist_Point]);
            Ycoord_Airfoil.push_back(Ycoord_Index1[MinDist_Point]);
            Zcoord_Airfoil.push_back(Zcoord_Index1[MinDist_Point]);
            Variable_Airfoil.push_back(Variable_Index1[MinDist_Point]);
          }
          
          if (Index == 1) {
            
            Xcoord_Airfoil.push_back(Xcoord_Index0[MinDist_Point]);
            Ycoord_Airfoil.push_back(Ycoord_Index0[MinDist_Point]);
            Zcoord_Airfoil.push_back(Zcoord_Index0[MinDist_Point]);
            Variable_Airfoil.push_back(Variable_Index0[MinDist_Point]);
          }
          
          Xcoord_Index0.erase(Xcoord_Index0.begin() + MinDist_Point);
          Ycoord_Index0.erase(Ycoord_Index0.begin() + MinDist_Point);
          Zcoord_Index0.erase(Zcoord_Index0.begin() + MinDist_Point);
          Variable_Index0.erase(Variable_Index0.begin() + MinDist_Point);
          
          Xcoord_Index1.erase(Xcoord_Index1.begin() + MinDist_Point);
          Ycoord_Index1.erase(Ycoord_Index1.begin() + MinDist_Point);
          Zcoord_Index1.erase(Zcoord_Index1.begin() + MinDist_Point);
          Variable_Index1.erase(Variable_Index1.begin() + MinDist_Point);
          
        }
        else { break; }
        
      } while (Xcoord_Index0.size() != 0);
      
      /*--- Clean the vector before using them again for storing the upper or the lower side ---*/
      
      Xcoord_Index0.clear(); Ycoord_Index0.clear(); Zcoord_Index0.clear(); Variable_Index0.clear();
      Xcoord_Index1.clear(); Ycoord_Index1.clear(); Zcoord_Index1.clear(); Variable_Index1.clear();
      
    }
    
  }
  
}

void CGeometry::RegisterCoordinates(CConfig *config) {
  unsigned short iDim;
  unsigned long iPoint;
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++) {
      AD::RegisterInput(node[iPoint]->GetCoord()[iDim]);
    }
  }
}

void CGeometry::UpdateGeometry(CGeometry **geometry_container, CConfig *config) {
  
  unsigned short iMesh;
  geometry_container[MESH_0]->Set_MPI_Coord(config);
  if (config->GetGrid_Movement()){
    geometry_container[MESH_0]->Set_MPI_GridVel(config);
  }
  
  geometry_container[MESH_0]->SetCoord_CG();
  geometry_container[MESH_0]->SetControlVolume(config, UPDATE);
  geometry_container[MESH_0]->SetBoundControlVolume(config, UPDATE);
  
  for (iMesh = 1; iMesh <= config->GetnMGLevels(); iMesh++) {
    /*--- Update the control volume structures ---*/
    
    geometry_container[iMesh]->SetControlVolume(config,geometry_container[iMesh-1], UPDATE);
    geometry_container[iMesh]->SetBoundControlVolume(config,geometry_container[iMesh-1], UPDATE);
    geometry_container[iMesh]->SetCoord(geometry_container[iMesh-1]);
    
  }
  
  if (config->GetKind_Solver() == DISC_ADJ_RANS)
  geometry_container[MESH_0]->ComputeWall_Distance(config);
  
}

void CGeometry::ComputeSurf_Curvature(CConfig *config) {
  unsigned short iMarker, iNeigh_Point, iDim, iNode, iNeighbor_Nodes, Neighbor_Node;
  unsigned long Neighbor_Point, iVertex, iPoint, jPoint, iElem_Bound, iEdge, nLocalVertex, MaxLocalVertex , *Buffer_Send_nVertex, *Buffer_Receive_nVertex, TotalnPointDomain;
  int iProcessor, nProcessor;
  vector<unsigned long> Point_NeighborList, Elem_NeighborList, Point_Triangle, Point_Edge, Point_Critical;
  vector<unsigned long>::iterator it;
  su2double U[3] = {0.0,0.0,0.0}, V[3] = {0.0,0.0,0.0}, W[3] = {0.0,0.0,0.0}, Length_U, Length_V, Length_W, CosValue, Angle_Value, *K, *Angle_Defect, *Area_Vertex, *Angle_Alpha, *Angle_Beta, **NormalMeanK, MeanK, GaussK, MaxPrinK, cot_alpha, cot_beta, delta, X1, X2, X3, Y1, Y2, Y3, radius, *Buffer_Send_Coord, *Buffer_Receive_Coord, *Coord, Dist, MinDist, MaxK, MinK, SigmaK;
  bool *Check_Edge;
  int rank;
  
#ifndef HAVE_MPI
  rank = MASTER_NODE;
#else
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  /*--- Allocate surface curvature ---*/
  K = new su2double [nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++) K[iPoint] = 0.0;
  
  if (nDim == 2) {
    
    /*--- Loop over all the markers ---*/
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      
      if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
        
        /*--- Loop through all marker vertices again, this time also
         finding the neighbors of each node.---*/
        for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
          iPoint  = vertex[iMarker][iVertex]->GetNode();
          
          if (node[iPoint]->GetDomain()) {
            /*--- Loop through neighbors. In 2-D, there should be 2 nodes on either
             side of this vertex that lie on the same surface. ---*/
            Point_Edge.clear();
            
            for (iNeigh_Point = 0; iNeigh_Point < node[iPoint]->GetnPoint(); iNeigh_Point++) {
              Neighbor_Point = node[iPoint]->GetPoint(iNeigh_Point);
              
              /*--- Check if this neighbor lies on the surface. If so,
               add to the list of neighbors. ---*/
              if (node[Neighbor_Point]->GetPhysicalBoundary()) {
                Point_Edge.push_back(Neighbor_Point);
              }
              
            }
            
            if (Point_Edge.size() == 2) {
              
              /*--- Compute the curvature using three points ---*/
              X1 = node[iPoint]->GetCoord(0);
              X2 = node[Point_Edge[0]]->GetCoord(0);
              X3 = node[Point_Edge[1]]->GetCoord(0);
              Y1 = node[iPoint]->GetCoord(1);
              Y2 = node[Point_Edge[0]]->GetCoord(1);
              Y3 = node[Point_Edge[1]]->GetCoord(1);
              
              radius = sqrt(((X2-X1)*(X2-X1) + (Y2-Y1)*(Y2-Y1))*
                            ((X2-X3)*(X2-X3) + (Y2-Y3)*(Y2-Y3))*
                            ((X3-X1)*(X3-X1) + (Y3-Y1)*(Y3-Y1)))/
              (2.0*fabs(X1*Y2+X2*Y3+X3*Y1-X1*Y3-X2*Y1-X3*Y2)+EPS);
              
              K[iPoint] = 1.0/radius;
              node[iPoint]->SetCurvature(K[iPoint]);
            }
            
          }
          
        }
        
      }
      
    }
    
  }
  
  else {
    
    Angle_Defect = new su2double [nPoint];
    Area_Vertex = new su2double [nPoint];
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      Angle_Defect[iPoint] = 2*PI_NUMBER;
      Area_Vertex[iPoint] = 0.0;
    }
    
    Angle_Alpha = new su2double [nEdge];
    Angle_Beta = new su2double [nEdge];
    Check_Edge = new bool [nEdge];
    for (iEdge = 0; iEdge < nEdge; iEdge++) {
      Angle_Alpha[iEdge] = 0.0;
      Angle_Beta[iEdge] = 0.0;
      Check_Edge[iEdge] = true;
    }
    
    NormalMeanK = new su2double *[nPoint];
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      NormalMeanK[iPoint] = new su2double [nDim];
      for (iDim = 0; iDim < nDim; iDim++) {
        NormalMeanK[iPoint][iDim] = 0.0;
      }
    }
    
    /*--- Loop over all the markers ---*/
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      
      if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
        
        /*--- Loop over all the boundary elements ---*/
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          
          /*--- Only triangles ---*/
          if (bound[iMarker][iElem_Bound]->GetVTK_Type() == TRIANGLE) {
            
            /*--- Loop over all the nodes of the boundary element ---*/
            for (iNode = 0; iNode < bound[iMarker][iElem_Bound]->GetnNodes(); iNode++) {
              
              iPoint = bound[iMarker][iElem_Bound]->GetNode(iNode);
              
              Point_Triangle.clear();
              
              for (iNeighbor_Nodes = 0; iNeighbor_Nodes < bound[iMarker][iElem_Bound]->GetnNeighbor_Nodes(iNode); iNeighbor_Nodes++) {
                Neighbor_Node = bound[iMarker][iElem_Bound]->GetNeighbor_Nodes(iNode, iNeighbor_Nodes);
                Neighbor_Point = bound[iMarker][iElem_Bound]->GetNode(Neighbor_Node);
                Point_Triangle.push_back(Neighbor_Point);
              }
              
              iEdge = FindEdge(Point_Triangle[0], Point_Triangle[1]);
              
              for (iDim = 0; iDim < nDim; iDim++) {
                U[iDim] = node[Point_Triangle[0]]->GetCoord(iDim) - node[iPoint]->GetCoord(iDim);
                V[iDim] = node[Point_Triangle[1]]->GetCoord(iDim) - node[iPoint]->GetCoord(iDim);
              }
              
              W[0] = 0.5*(U[1]*V[2]-U[2]*V[1]); W[1] = -0.5*(U[0]*V[2]-U[2]*V[0]); W[2] = 0.5*(U[0]*V[1]-U[1]*V[0]);
              
              Length_U = 0.0, Length_V = 0.0, Length_W = 0.0, CosValue = 0.0;
              for (iDim = 0; iDim < nDim; iDim++) { Length_U += U[iDim]*U[iDim]; Length_V += V[iDim]*V[iDim]; Length_W += W[iDim]*W[iDim]; }
              Length_U = sqrt(Length_U); Length_V = sqrt(Length_V); Length_W = sqrt(Length_W);
              for (iDim = 0; iDim < nDim; iDim++) { U[iDim] /= Length_U; V[iDim] /= Length_V; CosValue += U[iDim]*V[iDim]; }
              if (CosValue >= 1.0) CosValue = 1.0;
              if (CosValue <= -1.0) CosValue = -1.0;
              
              Angle_Value = acos(CosValue);
              Area_Vertex[iPoint] += Length_W;
              Angle_Defect[iPoint] -= Angle_Value;
              if (Angle_Alpha[iEdge] == 0.0) Angle_Alpha[iEdge] = Angle_Value;
              else Angle_Beta[iEdge] = Angle_Value;
              
            }
          }
        }
      }
    }
    
    /*--- Compute mean curvature ---*/
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          if (bound[iMarker][iElem_Bound]->GetVTK_Type() == TRIANGLE) {
            for (iNode = 0; iNode < bound[iMarker][iElem_Bound]->GetnNodes(); iNode++) {
              iPoint = bound[iMarker][iElem_Bound]->GetNode(iNode);
              
              for (iNeighbor_Nodes = 0; iNeighbor_Nodes < bound[iMarker][iElem_Bound]->GetnNeighbor_Nodes(iNode); iNeighbor_Nodes++) {
                Neighbor_Node = bound[iMarker][iElem_Bound]->GetNeighbor_Nodes(iNode, iNeighbor_Nodes);
                jPoint = bound[iMarker][iElem_Bound]->GetNode(Neighbor_Node);
                
                iEdge = FindEdge(iPoint, jPoint);
                
                if (Check_Edge[iEdge]) {
                  
                  Check_Edge[iEdge] = false;
                  
                  if (tan(Angle_Alpha[iEdge]) != 0.0) cot_alpha = 1.0/tan(Angle_Alpha[iEdge]); else cot_alpha = 0.0;
                  if (tan(Angle_Beta[iEdge]) != 0.0) cot_beta = 1.0/tan(Angle_Beta[iEdge]); else cot_beta = 0.0;
                  
                  /*--- iPoint, and jPoint ---*/
                  for (iDim = 0; iDim < nDim; iDim++) {
                    if (Area_Vertex[iPoint] != 0.0) NormalMeanK[iPoint][iDim] += 3.0 * (cot_alpha + cot_beta) * (node[iPoint]->GetCoord(iDim) - node[jPoint]->GetCoord(iDim)) / Area_Vertex[iPoint];
                    if (Area_Vertex[jPoint] != 0.0) NormalMeanK[jPoint][iDim] += 3.0 * (cot_alpha + cot_beta) * (node[jPoint]->GetCoord(iDim) - node[iPoint]->GetCoord(iDim)) / Area_Vertex[jPoint];
                  }
                }
                
              }
            }
          }
        }
      }
    }
    
    /*--- Compute Gauss, mean, max and min principal curvature,
     and set the list of critical points ---*/
    
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
        for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
          iPoint  = vertex[iMarker][iVertex]->GetNode();
          
          if (node[iPoint]->GetDomain()) {
            
            if (Area_Vertex[iPoint] != 0.0) GaussK = 3.0*Angle_Defect[iPoint]/Area_Vertex[iPoint];
            else GaussK = 0.0;
            
            MeanK = 0.0;
            for (iDim = 0; iDim < nDim; iDim++)
              MeanK += NormalMeanK[iPoint][iDim]*NormalMeanK[iPoint][iDim];
            MeanK = sqrt(MeanK);
            
            delta = max((MeanK*MeanK - GaussK), 0.0);
            
            MaxPrinK = MeanK + sqrt(delta);
            
            /*--- Store the curvature value ---*/
            K[iPoint] = MaxPrinK;
            node[iPoint]->SetCurvature(K[iPoint]);
          }
          
        }
      }
    }
    
    delete [] Angle_Defect;
    delete [] Area_Vertex;
    delete [] Angle_Alpha;
    delete [] Angle_Beta;
    delete [] Check_Edge;
    
    for (iPoint = 0; iPoint < nPoint; iPoint++)
      delete [] NormalMeanK[iPoint];
    delete [] NormalMeanK;
    
  }
  
  /*--- Sharp edge detection is based in the statistical
   distribution of the curvature ---*/
  
  MaxK = K[0]; MinK = K[0]; MeanK = 0.0; TotalnPointDomain = 0;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint  = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          MaxK = max(MaxK, fabs(K[iPoint]));
          MinK = min(MinK, fabs(K[iPoint]));
          MeanK += fabs(K[iPoint]);
          TotalnPointDomain++;
        }
      }
    }
  }
  
#ifdef HAVE_MPI
  su2double MyMeanK = MeanK; MeanK = 0.0;
  su2double MyMaxK = MaxK; MaxK = 0.0;
  unsigned long MynPointDomain = TotalnPointDomain; TotalnPointDomain = 0;
  SU2_MPI::Allreduce(&MyMeanK, &MeanK, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MyMaxK, &MaxK, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MynPointDomain, &TotalnPointDomain, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#endif
  
  /*--- Compute the mean ---*/
  MeanK /= su2double(TotalnPointDomain);
  
  /*--- Compute the standard deviation ---*/
  SigmaK = 0.0;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint  = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          SigmaK += (fabs(K[iPoint]) - MeanK) * (fabs(K[iPoint]) - MeanK);
        }
      }
    }
  }
  
#ifdef HAVE_MPI
  su2double MySigmaK = SigmaK; SigmaK = 0.0;
  SU2_MPI::Allreduce(&MySigmaK, &SigmaK, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
  
  SigmaK = sqrt(SigmaK/su2double(TotalnPointDomain));
  
  if (rank == MASTER_NODE)
    cout << "Max K: " << MaxK << ". Mean K: " << MeanK << ". Standard deviation K: " << SigmaK << "." << endl;
  
  Point_Critical.clear();
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint  = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          if (fabs(K[iPoint]) > MeanK + config->GetRefSharpEdges()*SigmaK) {
            Point_Critical.push_back(iPoint);
          }
        }
      }
    }
  }
  
  /*--- Variables and buffers needed for MPI ---*/
  
#ifdef HAVE_MPI
  MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
#else
  nProcessor = 1;
#endif
  
  Buffer_Send_nVertex    = new unsigned long [1];
  Buffer_Receive_nVertex = new unsigned long [nProcessor];
  
  /*--- Count the total number of critical edge nodes. ---*/
  
  nLocalVertex = Point_Critical.size();
  Buffer_Send_nVertex[0] = nLocalVertex;
  
  /*--- Communicate to all processors the total number of critical edge nodes. ---*/
  
#ifdef HAVE_MPI
  MaxLocalVertex = 0;
  SU2_MPI::Allreduce(&nLocalVertex, &MaxLocalVertex, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#else
  MaxLocalVertex = nLocalVertex;
  Buffer_Receive_nVertex[0] = nLocalVertex;
#endif
  
  
  /*--- Create and initialize to zero some buffers to hold the coordinates
   of the boundary nodes that are communicated from each partition (all-to-all). ---*/
  
  Buffer_Send_Coord     = new su2double [MaxLocalVertex*nDim];
  Buffer_Receive_Coord  = new su2double [nProcessor*MaxLocalVertex*nDim];
  
#ifdef HAVE_MPI
  unsigned long nBuffer               = MaxLocalVertex*nDim;
#endif

  for (iVertex = 0; iVertex < MaxLocalVertex; iVertex++) {
    for (iDim = 0; iDim < nDim; iDim++) {
      Buffer_Send_Coord[iVertex*nDim+iDim] = 0.0;
    }
  }
  
  /*--- Retrieve and store the coordinates of the sharp edges boundary nodes on
   the local partition and broadcast them to all partitions. ---*/
  
  for (iVertex = 0; iVertex < Point_Critical.size(); iVertex++) {
    iPoint = Point_Critical[iVertex];
    for (iDim = 0; iDim < nDim; iDim++)
      Buffer_Send_Coord[iVertex*nDim+iDim] = node[iPoint]->GetCoord(iDim);
  }
  
#ifdef HAVE_MPI
  SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer, MPI_DOUBLE, MPI_COMM_WORLD);
#else
  for (iVertex = 0; iVertex < Point_Critical.size(); iVertex++) {
    for (iDim = 0; iDim < nDim; iDim++) {
      Buffer_Receive_Coord[iVertex*nDim+iDim] = Buffer_Send_Coord[iVertex*nDim+iDim];
    }
  }
#endif
  
  /*--- Loop over all interior mesh nodes on the local partition and compute
   the distances to each of the no-slip boundary nodes in the entire mesh.
   Store the minimum distance to the wall for each interior mesh node. ---*/
  
  for (iPoint = 0; iPoint < GetnPoint(); iPoint++) {
    Coord = node[iPoint]->GetCoord();
    
    MinDist = 1E20;
    for (iProcessor = 0; iProcessor < nProcessor; iProcessor++) {
      for (iVertex = 0; iVertex < Buffer_Receive_nVertex[iProcessor]; iVertex++) {
        Dist = 0.0;
        for (iDim = 0; iDim < nDim; iDim++) {
          Dist += (Coord[iDim]-Buffer_Receive_Coord[(iProcessor*MaxLocalVertex+iVertex)*nDim+iDim])*
          (Coord[iDim]-Buffer_Receive_Coord[(iProcessor*MaxLocalVertex+iVertex)*nDim+iDim]);
        }
        if (Dist!=0.0) Dist = sqrt(Dist);
        else Dist = 0.0;
        if (Dist < MinDist) MinDist = Dist;
      }
    }
    node[iPoint]->SetSharpEdge_Distance(MinDist);
  }
  
  /*--- Deallocate Max curvature ---*/
  delete[] K;
  
  /*--- Deallocate the buffers needed for the MPI communication. ---*/
  delete[] Buffer_Send_Coord;
  delete[] Buffer_Receive_Coord;
  delete[] Buffer_Send_nVertex;
  delete[] Buffer_Receive_nVertex;
  
}

CPhysicalGeometry::CPhysicalGeometry() : CGeometry() {

  Local_to_Global_Point  = NULL;
  Local_to_Global_Marker = NULL;
  Global_to_Local_Marker = NULL;

  starting_node = NULL;
  ending_node   = NULL;
  npoint_procs  = NULL;

  /*--- Arrays for defining the tutbomachinery structure ---*/

  nSpanWiseSections       = NULL;
  SpanWiseValue           = NULL;
  nVertexSpan             = NULL;
  nTotVertexSpan          = NULL;
  turbovertex             = NULL;
  AverageTurboNormal      = NULL;
  AverageNormal           = NULL;
  AverageGridVel          = NULL;
  AverageTangGridVel      = NULL;
  SpanArea                = NULL;
  TurboRadius             = NULL;
  MaxAngularCoord         = NULL;
  MinAngularCoord         = NULL;
  MinRelAngularCoord      = NULL;

  TangGridVelIn           = NULL;
  SpanAreaIn              = NULL;
  TurboRadiusIn           = NULL;
  TangGridVelOut          = NULL;
  SpanAreaOut             = NULL;
  TurboRadiusOut          = NULL;
}

CPhysicalGeometry::CPhysicalGeometry(CConfig *config, unsigned short val_iZone, unsigned short val_nZone) : CGeometry() {
  
  Local_to_Global_Point = NULL;
  Local_to_Global_Marker = NULL;
  Global_to_Local_Marker = NULL;
  
  starting_node = NULL;
  ending_node   = NULL;
  npoint_procs  = NULL;
  
  /*--- Arrays for defining the tutbomachinery structure ---*/

  nSpanWiseSections       = NULL;
  SpanWiseValue           = NULL;
  nVertexSpan             = NULL;
  nTotVertexSpan          = NULL;
  turbovertex             = NULL;
  AverageTurboNormal      = NULL;
  AverageNormal           = NULL;
  AverageGridVel          = NULL;
  AverageTangGridVel      = NULL;
  SpanArea                = NULL;
  TurboRadius             = NULL;
  MaxAngularCoord         = NULL;
  MinAngularCoord         = NULL;
  MinRelAngularCoord      = NULL;

  TangGridVelIn           = NULL;
  SpanAreaIn              = NULL;
  TurboRadiusIn           = NULL;
  TangGridVelOut          = NULL;
  SpanAreaOut             = NULL;
  TurboRadiusOut          = NULL;

  string text_line, Marker_Tag;
  ifstream mesh_file;
  unsigned short iDim, iMarker, iNodes;
  unsigned long iPoint, iElem_Bound;
  su2double *NewCoord;
  nZone = val_nZone;
  ofstream boundary_file;
  string Grid_Marker;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  string val_mesh_filename  = config->GetMesh_FileName();
  unsigned short val_format = config->GetMesh_FileFormat();

  /*--- Initialize counters for local/global points & elements ---*/
  
  if (rank == MASTER_NODE)
    cout << endl <<"---------------------- Read Grid File Information -----------------------" << endl;
  
  switch (val_format) {
    case SU2:
      Read_SU2_Format_Parallel(config, val_mesh_filename, val_iZone, val_nZone);
      break;
    case CGNS:
      Read_CGNS_Format_Parallel(config, val_mesh_filename, val_iZone, val_nZone);
      break;
    default:
      if (rank == MASTER_NODE) cout << "Unrecognized mesh format specified!" << endl;
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
      break;
  }

  /*--- After reading the mesh, assert that the dimension is equal to 2 or 3. ---*/
  
  assert((nDim == 2) || (nDim == 3));
  
  /*--- Loop over the points element to re-scale the mesh, and plot it (only SU2_CFD) ---*/
  
  if (config->GetKind_SU2() == SU2_CFD) {
    
    NewCoord = new su2double [nDim];
    
    /*--- The US system uses feet, but SU2 assumes that the grid is in inches ---*/
    
    if (config->GetSystemMeasurements() == US) {
      for (iPoint = 0; iPoint < nPoint; iPoint++) {
        for (iDim = 0; iDim < nDim; iDim++) {
          NewCoord[iDim] = node[iPoint]->GetCoord(iDim)/12.0;
        }
        node[iPoint]->SetCoord(NewCoord);
      }
    }
    
    delete [] NewCoord;
    
  }
  
  /*--- If SU2_DEF then write a file with the boundary information ---*/
  
  if ((config->GetKind_SU2() == SU2_DEF) && (rank == MASTER_NODE)) {

    string str = "boundary.dat";

    str = config->GetMultizone_FileName(str, val_iZone);

    /*--- Open .su2 grid file ---*/
    
    boundary_file.open(str.c_str(), ios::out);
    
    /*--- Loop through and write the boundary info ---*/
    
    boundary_file << "NMARK= " << nMarker << endl;
    
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      
      Grid_Marker = config->GetMarker_All_TagBound(iMarker);
      boundary_file << "MARKER_TAG= " << Grid_Marker << endl;
      boundary_file << "MARKER_ELEMS= " << nElem_Bound[iMarker]<< endl;
      boundary_file << "SEND_TO= " << config->GetMarker_All_SendRecv(iMarker) << endl;
      if (nDim == 2) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          boundary_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
          for (iNodes = 0; iNodes < bound[iMarker][iElem_Bound]->GetnNodes(); iNodes++)
            boundary_file << bound[iMarker][iElem_Bound]->GetNode(iNodes) << "\t" ;

          if (bound[iMarker][iElem_Bound]->GetVTK_Type() == VERTEX) {
            boundary_file << bound[iMarker][iElem_Bound]->GetRotation_Type() << "\t";
          }
          boundary_file	<< iElem_Bound << endl;
        }
      }
      
      if (nDim == 3) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          boundary_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
          for (iNodes = 0; iNodes < bound[iMarker][iElem_Bound]->GetnNodes(); iNodes++)
            boundary_file << bound[iMarker][iElem_Bound]->GetNode(iNodes) << "\t" ;

          if (bound[iMarker][iElem_Bound]->GetVTK_Type() == VERTEX) {
            boundary_file << bound[iMarker][iElem_Bound]->GetRotation_Type() << "\t";
          }
          boundary_file	<< iElem_Bound << endl;
        }
      }
      
    }
    
    boundary_file.close();

  }
  
}

CPhysicalGeometry::CPhysicalGeometry(CGeometry *geometry, CConfig *config) {
  
  /*--- Initialize several class data members for later. ---*/
  
  Local_to_Global_Point  = NULL;
  Local_to_Global_Marker = NULL;
  Global_to_Local_Marker = NULL;
  
  starting_node = NULL;
  ending_node   = NULL;
  npoint_procs  = NULL;

  /*--- Arrays for defining the tutbomachinery structure ---*/

  nSpanWiseSections       = NULL;
  SpanWiseValue           = NULL;
  nVertexSpan             = NULL;
  nTotVertexSpan          = NULL;
  turbovertex             = NULL;
  AverageTurboNormal      = NULL;
  AverageNormal           = NULL;
  AverageGridVel          = NULL;
  AverageTangGridVel      = NULL;
  SpanArea                = NULL;
  TurboRadius             = NULL;
  MaxAngularCoord         = NULL;
  MinAngularCoord         = NULL;
  MinRelAngularCoord      = NULL;

  TangGridVelIn           = NULL;
  SpanAreaIn              = NULL;
  TurboRadiusIn           = NULL;
  TangGridVelOut          = NULL;
  SpanAreaOut             = NULL;
  TurboRadiusOut          = NULL;

  /*--- Local variables and counters for the following communications. ---*/
  
  unsigned long iter,  iPoint, jPoint, iElem, iVertex;
  unsigned long iElemTotal, iPointTotal, iPointGhost, iPointDomain, iPointPeriodic, iElemTriangle, iElemQuadrilateral, iElemTetrahedron, iElemHexahedron, iElemPrism, iElemPyramid, iPointCurrent;
  unsigned long nBoundLineTotal = 0, iBoundLineTotal;
  unsigned long nBoundTriangleTotal = 0, iBoundTriangleTotal;
  unsigned long nBoundQuadrilateralTotal = 0, iBoundQuadrilateralTotal;
  unsigned long ReceptorColor = 0, DonorColor = 0, Transformation;
  unsigned long nTotalSendDomain_Periodic = 0, iTotalSendDomain_Periodic, nTotalReceivedDomain_Periodic = 0, iTotalReceivedDomain_Periodic, *nSendDomain_Periodic = NULL, *nReceivedDomain_Periodic = NULL;
  unsigned long Buffer_Send_nPointTotal = 0, Buffer_Send_nPointDomainTotal = 0, Buffer_Send_nPointGhost = 0, Buffer_Send_nPointPeriodic = 0;
  unsigned long Buffer_Send_nElemTotal, Buffer_Send_nElemTriangle = 0, Buffer_Send_nElemQuadrilateral = 0, Buffer_Send_nElemTetrahedron = 0, Buffer_Send_nElemHexahedron = 0, Buffer_Send_nElemPrism = 0, Buffer_Send_nElemPyramid = 0;
  unsigned long Buffer_Send_nTotalSendDomain_Periodic = 0, Buffer_Send_nTotalReceivedDomain_Periodic = 0, *Buffer_Send_nSendDomain_Periodic = NULL, *Buffer_Send_nReceivedDomain_Periodic = NULL;
  unsigned long Buffer_Send_nBoundLineTotal = 0, Buffer_Send_nBoundTriangleTotal = 0, Buffer_Send_nBoundQuadrilateralTotal = 0;
  unsigned long iVertexDomain, iBoundLine, iBoundTriangle, iBoundQuadrilateral;
  
  unsigned long iNode, iDim, iMarker, jMarker, nMarkerDomain = 0, iMarkerDomain;
  unsigned long nDomain = 0, iDomain, jDomain, nPeriodic = 0, iPeriodic, Buffer_Send_nMarkerDomain = 0, Buffer_Send_nDim = 0, Buffer_Send_nZone = 0, Buffer_Send_nPeriodic = 0;
  
  bool *MarkerIn = NULL, **VertexIn = NULL, *ElemIn = NULL;
  long vnodes_local[8];
  
  vector<long> DomainList;
  short *Marker_All_SendRecv_Copy = NULL;
  string *Marker_All_TagBound_Copy = NULL;
  
  int rank = MASTER_NODE;
  int size = SINGLE_NODE;
  unsigned short nMarker_Max = config->GetnMarker_Max();
  
   
  /*--- Some dynamic arrays so we're not allocating too much on the stack ---*/
  
  unsigned long *nVertexDomain       = new unsigned long[nMarker_Max];
  unsigned long *nBoundLine          = new unsigned long[nMarker_Max];
  unsigned long *nBoundTriangle      = new unsigned long[nMarker_Max];
  unsigned long *nBoundQuadrilateral = new unsigned long[nMarker_Max];
  
  unsigned long *Buffer_Send_nVertexDomain       = new unsigned long[nMarker_Max];
  unsigned long *Buffer_Send_nBoundLine          = new unsigned long[nMarker_Max];
  unsigned long *Buffer_Send_nBoundTriangle      = new unsigned long[nMarker_Max];
  unsigned long *Buffer_Send_nBoundQuadrilateral = new unsigned long[nMarker_Max];
  
  short *Buffer_Send_Marker_All_SendRecv = new short[nMarker_Max];
  
  char *Marker_All_TagBound             = new char[nMarker_Max*MAX_STRING_SIZE];
  char *Buffer_Send_Marker_All_TagBound = new char[nMarker_Max*MAX_STRING_SIZE];
  
#ifdef HAVE_MPI
  
  /*--- MPI initialization ---*/
  
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  /*--- MPI status and request arrays for non-blocking communications ---*/
  
  MPI_Status status, status2;
  unsigned long source;
  int recv_count=0;
  
  int offset = 17;
  MPI_Status *send_stat = new MPI_Status[offset+size];
  MPI_Status *recv_stat = new MPI_Status[offset+size];
  
  MPI_Request *send_req = new MPI_Request[offset+size];
  MPI_Request *recv_req = new MPI_Request[offset+size];
  
#endif
  
  if (rank == MASTER_NODE && size > SINGLE_NODE)
    cout << "Communicating partition data and creating halo layers." << endl;
  
  /*--- Define buffer vectors for the interior points / elements ---*/
  
  su2double *Buffer_Send_Coord = NULL;
  
  unsigned long *Buffer_Send_Color            = NULL;
  unsigned long *Buffer_Send_GlobalPointIndex = NULL;
  unsigned long *Buffer_Send_Triangle         = NULL;
  unsigned long *Buffer_Send_Quadrilateral    = NULL;
  unsigned long *Buffer_Send_Tetrahedron      = NULL;
  unsigned long *Buffer_Send_Hexahedron       = NULL;
  unsigned long *Buffer_Send_Prism            = NULL;
  unsigned long *Buffer_Send_Pyramid          = NULL;
  unsigned long *Buffer_Send_GlobElem         = NULL;
  
  /*--- Define buffer vectors for boundary information ---*/
  
  unsigned long *Buffer_Send_BoundLine = NULL,           *Buffer_Receive_BoundLine = NULL;
  unsigned long *Buffer_Send_BoundTriangle = NULL,       *Buffer_Receive_BoundTriangle = NULL;
  unsigned long *Buffer_Send_BoundQuadrilateral = NULL,  *Buffer_Receive_BoundQuadrilateral = NULL;
  unsigned long *Buffer_Send_Local2Global_Marker = NULL, *Buffer_Receive_Local2Global_Marker = NULL;
  
  /*--- Define buffer vectors for periodic boundary conditions ---*/
  
  su2double *Buffer_Send_Center = NULL,    *Buffer_Receive_Center = NULL;
  su2double *Buffer_Send_Rotation = NULL,  *Buffer_Receive_Rotation = NULL;
  su2double *Buffer_Send_Translate = NULL, *Buffer_Receive_Translate = NULL;
  
  /*--- Define buffer vector periodic boundary conditions ---*/
  
  unsigned long *Buffer_Send_SendDomain_Periodic = NULL,          *Buffer_Receive_SendDomain_Periodic = NULL;
  unsigned long *Buffer_Send_SendDomain_PeriodicTrans = NULL,     *Buffer_Receive_SendDomain_PeriodicTrans = NULL;
  unsigned long *Buffer_Send_SendDomain_PeriodicReceptor = NULL,  *Buffer_Receive_SendDomain_PeriodicReceptor = NULL;
  unsigned long *Buffer_Send_ReceivedDomain_Periodic = NULL,      *Buffer_Receive_ReceivedDomain_Periodic = NULL;
  unsigned long *Buffer_Send_ReceivedDomain_PeriodicTrans = NULL, *Buffer_Receive_ReceivedDomain_PeriodicTrans = NULL;
  unsigned long *Buffer_Send_ReceivedDomain_PeriodicDonor = NULL, *Buffer_Receive_ReceivedDomain_PeriodicDonor = NULL;
  
  /*--- Variables below are needed specifically for the ParMETIS version ---*/
  
  unsigned short *nDim_s  = new unsigned short[size];
  unsigned short *nDim_r  = new unsigned short[size];
  unsigned short *nZone_s = new unsigned short[size];
  unsigned short *nZone_r = new unsigned short[size];
  
  unsigned long *nPointTotal_s        = new unsigned long[size];
  unsigned long *nPointDomainTotal_s  = new unsigned long[size];
  unsigned long *nPointGhost_s        = new unsigned long[size];
  unsigned long *nPointPeriodic_s     = new unsigned long[size];
  unsigned long *nElemTotal_s         = new unsigned long[size];
  unsigned long *nElemTriangle_s      = new unsigned long[size];
  unsigned long *nElemQuadrilateral_s = new unsigned long[size];
  unsigned long *nElemTetrahedron_s   = new unsigned long[size];
  unsigned long *nElemHexahedron_s    = new unsigned long[size];
  unsigned long *nElemPrism_s         = new unsigned long[size];
  unsigned long *nElemPyramid_s       = new unsigned long[size];
  
  unsigned long *nPointTotal_r        = new unsigned long[size];
  unsigned long *nPointDomainTotal_r  = new unsigned long[size];
  unsigned long *nPointGhost_r        = new unsigned long[size];
  unsigned long *nPointPeriodic_r     = new unsigned long[size];
  unsigned long *nElemTotal_r         = new unsigned long[size];
  unsigned long *nElemTriangle_r      = new unsigned long[size];
  unsigned long *nElemQuadrilateral_r = new unsigned long[size];
  unsigned long *nElemTetrahedron_r   = new unsigned long[size];
  unsigned long *nElemHexahedron_r    = new unsigned long[size];
  unsigned long *nElemPrism_r         = new unsigned long[size];
  unsigned long *nElemPyramid_r       = new unsigned long[size];
  
  /*--- Counters needed to track numbers of points, elements, etc. ---*/
  
  unsigned long nPointTotal_r_tot=0;
  unsigned long nPointDomainTotal_r_tot=0;
  unsigned long nPointGhost_r_tot=0;
  unsigned long nPointPeriodic_r_tot=0;
  unsigned long nElemTotal_r_tot=0;
  unsigned long nElemTriangle_r_tot=0;
  unsigned long nElemQuadrilateral_r_tot=0;
  unsigned long nElemTetrahedron_r_tot=0;
  unsigned long nElemHexahedron_r_tot=0;
  unsigned long nElemPrism_r_tot=0;
  unsigned long nElemPyramid_r_tot=0;
  
  unsigned long Buffer_Size_Coord = 0;
  unsigned long Buffer_Size_Color = 0;
  unsigned long Buffer_Size_GlobalPointIndex = 0;
  unsigned long Buffer_Size_Triangle = 0;
  unsigned long Buffer_Size_Quadrilateral = 0;
  unsigned long Buffer_Size_Tetrahedron = 0;
  unsigned long Buffer_Size_Hexahedron = 0;
  unsigned long Buffer_Size_Prism = 0;
  unsigned long Buffer_Size_Pyramid = 0;
  unsigned long Buffer_Size_GlobElem = 0;
  
  unsigned long ElemTotal_Counter = 0;
  unsigned long PointTotal_Counter = 0;
  unsigned long PointDomain_Counter = 0;
  
  /*--- WARNING: check the next two counters ---*/
  unsigned long PointPeriodic_Counter = 0;
  unsigned long PointGhost_Counter = 0;
  unsigned long ElemTriangle_Counter = 0;
  unsigned long ElemQuadrilateral_Counter = 0;
  unsigned long ElemTetrahedron_Counter = 0;
  unsigned long ElemHexahedron_Counter = 0;
  unsigned long ElemPrism_Counter = 0;
  unsigned long ElemPyramid_Counter = 0;
  
  unsigned long *Local_to_global_Triangle      = NULL;
  unsigned long *Local_to_global_Quadrilateral = NULL;
  unsigned long *Local_to_global_Tetrahedron   = NULL;
  unsigned long *Local_to_global_Hexahedron    = NULL;
  unsigned long *Local_to_global_Prism         = NULL;
  unsigned long *Local_to_global_Pyramid       = NULL;
  
  map<unsigned long,bool> Triangle_presence;
  map<unsigned long,bool> Quadrilateral_presence;
  map<unsigned long,bool> Tetrahedron_presence;
  map<unsigned long,bool> Hexahedron_presence;
  map<unsigned long,bool> Prism_presence;
  map<unsigned long,bool> Pyramid_presence;
  
  su2double *Buffer_Receive_Coord_loc = NULL;
  
  unsigned long *Buffer_Receive_Color_loc            = NULL;
  unsigned long *Buffer_Receive_GlobalPointIndex_loc = NULL;
  unsigned long *Buffer_Receive_Triangle_loc         = NULL;
  unsigned long *Buffer_Receive_Quadrilateral_loc    = NULL;
  unsigned long *Buffer_Receive_Tetrahedron_loc      = NULL;
  unsigned long *Buffer_Receive_Hexahedron_loc       = NULL;
  unsigned long *Buffer_Receive_Prism_loc            = NULL;
  unsigned long *Buffer_Receive_Pyramid_loc          = NULL;
  
  unsigned long *Buffer_Receive_GlobElem_loc               = NULL;
  unsigned long *Buffer_Receive_Triangle_presence_loc      = NULL;
  unsigned long *Buffer_Receive_Quadrilateral_presence_loc = NULL;
  unsigned long *Buffer_Receive_Tetrahedron_presence_loc   = NULL;
  unsigned long *Buffer_Receive_Hexahedron_presence_loc    = NULL;
  unsigned long *Buffer_Receive_Prism_presence_loc         = NULL;
  unsigned long *Buffer_Receive_Pyramid_presence_loc       = NULL;
  
  /*--- Allocate the memory that we only need if we have MPI support ---*/
  
#ifdef HAVE_MPI
  
  su2double *Buffer_Receive_Coord = NULL;
  
  unsigned long *Buffer_Receive_Color            = NULL;
  unsigned long *Buffer_Receive_GlobalPointIndex = NULL;
  unsigned long *Buffer_Receive_Triangle         = NULL;
  unsigned long *Buffer_Receive_Quadrilateral    = NULL;
  unsigned long *Buffer_Receive_Tetrahedron      = NULL;
  unsigned long *Buffer_Receive_Hexahedron       = NULL;
  unsigned long *Buffer_Receive_Prism            = NULL;
  unsigned long *Buffer_Receive_Pyramid          = NULL;
  unsigned long *Buffer_Receive_GlobElem         = NULL;
  
  unsigned long **Buffer_Receive_Triangle_presence      = new unsigned long*[size];
  unsigned long **Buffer_Receive_Quadrilateral_presence = new unsigned long*[size];
  unsigned long **Buffer_Receive_Tetrahedron_presence   = new unsigned long*[size];
  unsigned long **Buffer_Receive_Hexahedron_presence    = new unsigned long*[size];
  unsigned long **Buffer_Receive_Prism_presence         = new unsigned long*[size];
  unsigned long **Buffer_Receive_Pyramid_presence       = new unsigned long*[size];
  
#endif
  
  /*--- Basic dimensionalization ---*/
  
  nDomain = size;
  
  Marker_All_SendRecv      = new short[nMarker_Max];
  nSendDomain_Periodic     = new unsigned long [nDomain];
  nReceivedDomain_Periodic = new unsigned long [nDomain];
  
  /*--- Auxiliar vector based on the original geometry ---*/
  
  ElemIn = new bool[geometry->GetnElem()];
  
  /*--- Define some mapping variables ---*/
  map<unsigned long,bool> PointIn;
  map<unsigned long,unsigned long> Global_to_local_Point_recv;
  
  Buffer_Send_nDim  = geometry->GetnDim();
  Buffer_Send_nZone = geometry->GetnZone();
  
  /*--- Divide the elements in color list to speed up the grid partitioning ---*/
  
  map<unsigned long,unsigned long> Local_to_global_elem;
  for (unsigned long i=0; i<geometry->GetGlobal_nElem(); i++) {
    map<unsigned long, unsigned long>::const_iterator MI = geometry->Global_to_Local_Elem.find(i);
    if (MI != geometry->Global_to_Local_Elem.end()) {
      Local_to_global_elem[geometry->Global_to_Local_Elem[i]] = i;
    }
  }
  
  /*--- MEMORY WARNING: Bad usage of memory here for local_colour_values. Not scalable. 
        In the future, we should avoid sharing a single array of all colors in all nodes. ---*/
  unsigned long *local_colour_values = new unsigned long[geometry->GetGlobal_nPoint()];
  unsigned long *local_colour_temp   = new unsigned long[geometry->ending_node[rank]-geometry->starting_node[rank]];
  
  for (unsigned long i=0; i<geometry->ending_node[rank]-geometry->starting_node[rank]; i++) {
    local_colour_temp[i]=geometry->node[i]->GetColor();
    local_colour_values[geometry->starting_node[rank]+i]=local_colour_temp[i];
  }
  
  /*--- Communicate the grid coloring to all partitions. This information
   will be repeatedly used throughout the organization of the partitions
   and sorting out their ghost points/elements. ---*/
  
#ifdef HAVE_MPI
  
  int comm_counter=0;
  for (iDomain=0; iDomain < (unsigned long)size; iDomain++) {
    if (iDomain != (unsigned long)rank) {
      SU2_MPI::Isend(local_colour_temp, geometry->ending_node[rank]-geometry->starting_node[rank],
                     MPI_UNSIGNED_LONG, iDomain, iDomain,  MPI_COMM_WORLD, &send_req[comm_counter]);
      comm_counter++;
    }
  }
  
  for (iDomain=0; iDomain < (unsigned long)size-1; iDomain++) {
    MPI_Probe(MPI_ANY_SOURCE, rank, MPI_COMM_WORLD, &status2);
    source = status2.MPI_SOURCE;
    MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
    SU2_MPI::Recv(&local_colour_values[geometry->starting_node[source]], recv_count,
                  MPI_UNSIGNED_LONG, source, rank, MPI_COMM_WORLD, &status2);
  }
  
  /*--- Wait for the sends to complete (will be true since we're using
   blocking recv's above. ---*/
  
  SU2_MPI::Waitall(size-1, send_req, send_stat);
  
#endif
  
  /*--- Free temporary buffer for communicating colors. ---*/
  
  delete [] local_colour_temp;
  
#ifdef HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  /*--- This loop gets the array sizes of points, elements, etc. for each
   rank to send to each other rank. ---*/
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    /*--- Interior dimensionalization. Loop over the original grid to
     perform the dimensionalizaton of the domain variables ---*/
    
    Buffer_Send_nElemTotal         = 0;
    Buffer_Send_nPointTotal        = 0;
    Buffer_Send_nPointGhost        = 0;
    Buffer_Send_nPointDomainTotal  = 0;
    Buffer_Send_nPointPeriodic     = 0;
    Buffer_Send_nElemTriangle      = 0;
    Buffer_Send_nElemQuadrilateral = 0;
    Buffer_Send_nElemTetrahedron   = 0;
    Buffer_Send_nElemHexahedron    = 0;
    Buffer_Send_nElemPrism         = 0;
    Buffer_Send_nElemPyramid       = 0;
    
    /*--- Initialize the global to local mapping ---*/
    
    PointIn.clear();
    
    /*--- Loop over all of the local elements and count the number of each
     type of point and element that needs to be sent. ---*/
    
    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
      
      /*--- Check if the element belongs to the domain ---*/
      
      ElemIn[iElem] = false;
      for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++) {
        iPoint = geometry->elem[iElem]->GetNode(iNode);
        if (local_colour_values[iPoint] == iDomain) {
          ElemIn[iElem] = true; break;
        }
      }
      
      /*--- If this element is needed by iDomain, get information
       about the number of points and element type. ---*/
      
      if (ElemIn[iElem]) {
        
        for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++) {
          iPoint = geometry->elem[iElem]->GetNode(iNode);
          
          /*--- If we haven't already found this point... ---*/
          
          map<unsigned long, bool>::const_iterator MI = PointIn.find(iPoint);
          if (MI == PointIn.end()) {
            
            /*--- Mark point as found and collect information ---*/
            
            PointIn[iPoint] = true;
            
            if ((iPoint >= geometry->starting_node[rank]) &&
                (iPoint < geometry->ending_node[rank])) {
              
              Buffer_Send_nPointTotal++;
              
              /*--- Increment our counters ---*/
              if ( local_colour_values[iPoint] == iDomain ) {
                if ( iPoint > geometry->GetGlobal_nPointDomain() - 1)
                  Buffer_Send_nPointPeriodic++;
                else
                  Buffer_Send_nPointDomainTotal++;
              }
              else Buffer_Send_nPointGhost++;
              
              
            }
          }
        }
        
        /*--- Increment the counter for the current type of element ---*/
        
        switch(geometry->elem[iElem]->GetVTK_Type()) {
          case TRIANGLE:      Buffer_Send_nElemTriangle++;      break;
          case QUADRILATERAL: Buffer_Send_nElemQuadrilateral++; break;
          case TETRAHEDRON:   Buffer_Send_nElemTetrahedron++;   break;
          case HEXAHEDRON:    Buffer_Send_nElemHexahedron++;    break;
          case PRISM:         Buffer_Send_nElemPrism++;         break;
          case PYRAMID:       Buffer_Send_nElemPyramid++;       break;
        }
        
        /*--- Increment the total number of elements for iDomain ---*/
        
        Buffer_Send_nElemTotal++;
        
      }
    }
    
    /*--- Store the counts on a partition by partition basis. ---*/
    
    nDim_s[iDomain]               = geometry->GetnDim();
    nZone_s[iDomain]              = Buffer_Send_nZone;
    nPointTotal_s[iDomain]        = Buffer_Send_nPointTotal;
    nPointDomainTotal_s[iDomain]  = Buffer_Send_nPointDomainTotal;
    nPointGhost_s[iDomain]        = Buffer_Send_nPointGhost;
    nPointPeriodic_s[iDomain]     = Buffer_Send_nPointPeriodic;
    nElemTotal_s[iDomain]         = Buffer_Send_nElemTotal;
    nElemTriangle_s[iDomain]      = Buffer_Send_nElemTriangle;
    nElemQuadrilateral_s[iDomain] = Buffer_Send_nElemQuadrilateral;
    nElemTetrahedron_s[iDomain]   = Buffer_Send_nElemTetrahedron;
    nElemHexahedron_s[iDomain]    = Buffer_Send_nElemHexahedron;
    nElemPrism_s[iDomain]         = Buffer_Send_nElemPrism;
    nElemPyramid_s[iDomain]       = Buffer_Send_nElemPyramid;
    
    /*--- Total counts for allocating send buffers below ---*/
    
    Buffer_Size_Coord            += nPointTotal_s[iDomain]*nDim_s[iDomain];
    Buffer_Size_Color            += nPointTotal_s[iDomain];
    Buffer_Size_GlobalPointIndex += nPointTotal_s[iDomain];
    Buffer_Size_Triangle         += nElemTriangle_s[iDomain];
    Buffer_Size_Quadrilateral    += nElemQuadrilateral_s[iDomain];
    Buffer_Size_Tetrahedron      += nElemTetrahedron_s[iDomain];
    Buffer_Size_Hexahedron       += nElemHexahedron_s[iDomain];
    Buffer_Size_Prism            += nElemPrism_s[iDomain];
    Buffer_Size_Pyramid          += nElemPyramid_s[iDomain];
    Buffer_Size_GlobElem         += nElemTotal_s[iDomain];
    
  }
  
  /*--- Allocate the buffer vectors in the appropiate domain (master, iDomain) ---*/
  
  Buffer_Send_Coord = new su2double[Buffer_Size_Coord];
  
  Buffer_Send_Color             = new unsigned long[Buffer_Size_Color];
  Buffer_Send_GlobalPointIndex  = new unsigned long[Buffer_Size_GlobalPointIndex];
  Buffer_Send_Triangle          = new unsigned long[Buffer_Size_Triangle*N_POINTS_TRIANGLE];
  Buffer_Send_Quadrilateral     = new unsigned long[Buffer_Size_Quadrilateral*N_POINTS_QUADRILATERAL];
  Buffer_Send_Tetrahedron       = new unsigned long[Buffer_Size_Tetrahedron*N_POINTS_TETRAHEDRON];
  Buffer_Send_Hexahedron        = new unsigned long[Buffer_Size_Hexahedron*N_POINTS_HEXAHEDRON];
  Buffer_Send_Prism             = new unsigned long[Buffer_Size_Prism*N_POINTS_PRISM];
  Buffer_Send_Pyramid           = new unsigned long[Buffer_Size_Pyramid*N_POINTS_PYRAMID];
  Buffer_Send_GlobElem          = new unsigned long[Buffer_Size_GlobElem];
  
  Local_to_global_Triangle      = new unsigned long[Buffer_Size_Triangle];
  Local_to_global_Quadrilateral = new unsigned long[Buffer_Size_Quadrilateral];
  Local_to_global_Tetrahedron   = new unsigned long[Buffer_Size_Tetrahedron];
  Local_to_global_Hexahedron    = new unsigned long[Buffer_Size_Hexahedron];
  Local_to_global_Prism         = new unsigned long[Buffer_Size_Prism];
  Local_to_global_Pyramid       = new unsigned long[Buffer_Size_Pyramid];
  
  /*--- Initialize the counters for the larger send buffers (by domain) ---*/
  
  ElemTotal_Counter         = 0;
  PointTotal_Counter        = 0;
  PointDomain_Counter       = 0;
  /*--- WARNING: check the next two counters ---*/
  PointPeriodic_Counter     = 0;
  PointGhost_Counter        = 0;
  ElemTriangle_Counter      = 0;
  ElemQuadrilateral_Counter = 0;
  ElemTetrahedron_Counter   = 0;
  ElemHexahedron_Counter    = 0;
  ElemPrism_Counter         = 0;
  ElemPyramid_Counter       = 0;
  
  /*--- Now that we know the sizes of the point, elem, etc. arrays, we can
   allocate and send the information in large chunks to all processors. ---*/
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    /*--- A rank does not communicate with itself through MPI ---*/
    
    if ((unsigned long)rank != iDomain) {
      
#ifdef HAVE_MPI
      
      /*--- Communicate the counts to iDomain with non-blocking sends ---*/
      
      SU2_MPI::Isend(&nDim_s[iDomain], 1, MPI_UNSIGNED_SHORT, iDomain,
                     iDomain*13+0, MPI_COMM_WORLD, &send_req[0]);
      
      SU2_MPI::Isend(&nZone_s[iDomain], 1, MPI_UNSIGNED_SHORT, iDomain,
                     iDomain*13+1, MPI_COMM_WORLD, &send_req[1]);
      
      SU2_MPI::Isend(&nPointTotal_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+2, MPI_COMM_WORLD, &send_req[2]);
      
      SU2_MPI::Isend(&nPointDomainTotal_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+3, MPI_COMM_WORLD, &send_req[3]);
      
      SU2_MPI::Isend(&nPointGhost_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+4, MPI_COMM_WORLD, &send_req[4]);
      
      SU2_MPI::Isend(&nPointPeriodic_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+5, MPI_COMM_WORLD, &send_req[5]);
      
      SU2_MPI::Isend(&nElemTotal_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+6, MPI_COMM_WORLD, &send_req[6]);
      
      SU2_MPI::Isend(&nElemTriangle_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+7, MPI_COMM_WORLD, &send_req[7]);
      
      SU2_MPI::Isend(&nElemQuadrilateral_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+8, MPI_COMM_WORLD, &send_req[8]);
      
      SU2_MPI::Isend(&nElemTetrahedron_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+9, MPI_COMM_WORLD, &send_req[9]);
      
      SU2_MPI::Isend(&nElemHexahedron_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+10, MPI_COMM_WORLD, &send_req[10]);
      
      SU2_MPI::Isend(&nElemPrism_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+11, MPI_COMM_WORLD, &send_req[11]);
      
      SU2_MPI::Isend(&nElemPyramid_s[iDomain], 1, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*13+12, MPI_COMM_WORLD, &send_req[12]);
      
#endif
      
    } else {
      
      /*--- If iDomain = rank, we simply copy values into place in memory ---*/
      
      nDim              = nDim_s[iDomain];
      nZone             = nZone_s[iDomain];
      
      //      nPointTotal        = nPointTotal_s[iDomain];
      //      nPointDomainTotal  = nPointDomainTotal_s[iDomain];
      //      nPointGhost        = nPointGhost_s[iDomain];
      //      nPointPeriodic     = nPointPeriodic_s[iDomain];
      //      nElemTotal         = nElemTotal_s[iDomain];
      //      nElemTriangle      = nElemTriangle_s[iDomain];
      //      nElemQuadrilateral = nElemQuadrilateral_s[iDomain];
      //      nElemTetrahedron   = nElemTetrahedron_s[iDomain];
      //      nElemHexahedron    = nElemHexahedron_s[iDomain];
      //      nElemPrism         = nElemPrism_s[iDomain];
      //      nElemPyramid       = nElemPyramid_s[iDomain];
      
      nDim_r[iDomain]               = nDim_s[iDomain];
      nZone_r[iDomain]              = nZone_s[iDomain];
      nPointTotal_r[iDomain]        = nPointTotal_s[iDomain];
      nPointDomainTotal_r[iDomain]  = nPointDomainTotal_s[iDomain];
      nPointPeriodic_r[iDomain]     = nPointPeriodic_s[iDomain];
      nElemTotal_r[iDomain]         = nElemTotal_s[iDomain];
      nElemTriangle_r[iDomain]      = nElemTriangle_s[iDomain];
      nElemQuadrilateral_r[iDomain] = nElemQuadrilateral_s[iDomain];
      nElemTetrahedron_r[iDomain]   = nElemTetrahedron_s[iDomain];
      nElemHexahedron_r[iDomain]    = nElemHexahedron_s[iDomain];
      nElemPrism_r[iDomain]         = nElemPrism_s[iDomain];
      nElemPyramid_r[iDomain]       = nElemPyramid_s[iDomain];
      
      nPointTotal_r_tot        += nPointTotal_r[iDomain];
      nPointDomainTotal_r_tot  += nPointDomainTotal_r[iDomain];
      nPointGhost_r_tot        += nPointGhost_r[iDomain];
      nPointPeriodic_r_tot     += nPointPeriodic_r[iDomain];
      nElemTotal_r_tot         += nElemTotal_r[iDomain];
      nElemTriangle_r_tot      += nElemTriangle_r[iDomain];
      nElemQuadrilateral_r_tot += nElemQuadrilateral_r[iDomain];
      nElemTetrahedron_r_tot   += nElemTetrahedron_r[iDomain];
      nElemHexahedron_r_tot    += nElemHexahedron_r[iDomain];
      nElemPrism_r_tot         += nElemPrism_r[iDomain];
      nElemPyramid_r_tot       += nElemPyramid_r[iDomain];
      
    }
    
    /*--- Receive the counts. All processors are sending their counters to
     iDomain up above, so only iDomain needs to perform the recv here from
     all other ranks. ---*/
    
    if ((unsigned long)rank == iDomain) {
      
      for (jDomain = 0; jDomain < (unsigned long)size; jDomain++) {
        
        /*--- A rank does not communicate with itself through MPI ---*/
        
        if ((unsigned long)rank != jDomain) {
          
#ifdef HAVE_MPI
          
          /*--- Recv the data by probing for the current sender, jDomain,
           first and then receiving the values from it. ---*/
          
          MPI_Probe(jDomain, 13*rank+0, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nDim_r[jDomain], 1, MPI_UNSIGNED_SHORT, jDomain,
                        rank*13+0, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+1, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nZone_r[jDomain], 1, MPI_UNSIGNED_SHORT, jDomain,
                        rank*13+1, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+2, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nPointTotal_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+2, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+3, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nPointDomainTotal_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+3, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+4, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nPointGhost_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+4, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+5, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nPointPeriodic_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+5, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+6, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemTotal_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+6, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+7, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemTriangle_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+7, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+8, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemQuadrilateral_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+8, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+9, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemTetrahedron_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+9, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+10, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemHexahedron_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+10, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+11, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemPrism_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+11, MPI_COMM_WORLD, &status2);
          
          MPI_Probe(jDomain, 13*rank+12, MPI_COMM_WORLD, &status2);
          SU2_MPI::Recv(&nElemPyramid_r[jDomain], 1, MPI_UNSIGNED_LONG, jDomain,
                        rank*13+12, MPI_COMM_WORLD, &status2);
          
#endif
          
          /*--- These are the cumulative totals that we will recv below. ----*/
          
          nPointTotal_r_tot        += nPointTotal_r[jDomain];
          nPointDomainTotal_r_tot  += nPointDomainTotal_r[jDomain];
          nPointGhost_r_tot        += nPointGhost_r[jDomain];
          nPointPeriodic_r_tot     += nPointPeriodic_r[jDomain];
          nElemTotal_r_tot         += nElemTotal_r[jDomain];
          nElemTriangle_r_tot      += nElemTriangle_r[jDomain];
          nElemQuadrilateral_r_tot += nElemQuadrilateral_r[jDomain];
          nElemTetrahedron_r_tot   += nElemTetrahedron_r[jDomain];
          nElemHexahedron_r_tot    += nElemHexahedron_r[jDomain];
          nElemPrism_r_tot         += nElemPrism_r[jDomain];
          nElemPyramid_r_tot       += nElemPyramid_r[jDomain];
          
        }
      }
      
    }
  }
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    /*--- Wait for the non-blocking sends to complete. ---*/
    
#ifdef HAVE_MPI
    if ((unsigned long)rank != iDomain) SU2_MPI::Waitall(13, send_req, send_stat);
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    
  }
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    /*--- Above was number of elements to send and receive, and here is where
     we send/recv the actual elements. Here you're sending global index values,
     which are later changed to local. ---*/
    
    /*--- Set the value of the interior geometry. Initialize counters. ---*/
    
    iElemTotal       = 0;
    iPointTotal      = 0;
    iPointDomain     = 0;
    iPointPeriodic   = nPointDomainTotal_s[iDomain];
    iPointGhost      = nPointDomainTotal_s[iDomain] + nPointPeriodic_s[iDomain];
    iElemTriangle    = 0;
    iElemQuadrilateral   = 0;
    iElemTetrahedron = 0;
    iElemHexahedron  = 0;
    iElemPrism       = 0;
    iElemPyramid     = 0;
    
    /*--- Initialize the global to local mapping ---*/
    
    PointIn.clear();
    
    /*--- Load up the actual elements into the buffers for sending. ---*/
    
    for (iElem = 0; iElem < geometry->GetnElem(); iElem++) {
      
      /*--- Check if the element belongs to the domain ---*/
      
      ElemIn[iElem] = false;
      for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++) {
        iPoint = geometry->elem[iElem]->GetNode(iNode);
        if (local_colour_values[iPoint] == iDomain) {
          ElemIn[iElem] = true; break;
        }
      }
      
      /*--- If this element should be sent ---*/
      
      if (ElemIn[iElem]) {
        
        /*--- We need to send this element, so add it to the send buffer. The
         local to global mapping has already been done as a class data member. ---*/
        
        Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal] = Local_to_global_elem[iElem];
        
        /*--- Loop through the nodes of the current element ---*/
        
        for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++) {
          
          /*--- Get the global index for this node in the element ---*/
          iPoint = geometry->elem[iElem]->GetNode(iNode);
          
          /*--- Store the connectivity for this element for each node ---*/
          vnodes_local[iNode] = iPoint;
          
          /*--- Check if this point has been found previously ---*/
          
          map<unsigned long, bool>::const_iterator MI = PointIn.find(iPoint);
          if (MI == PointIn.end()) {
            
            /*--- Check if this node lives on the current rank based on the
             initial linear partitioning. We are only ever sending nodes that
             we own in the linear partitioning (no duplicate nodes are sent) ---*/
            
            if ((iPoint >= geometry->starting_node[rank]) &&
                (iPoint < geometry->ending_node[rank])) {
              
              /*--- Decide whether this is an interior, periodic, or ghost node ---*/
              
              if (local_colour_values[iPoint] == iDomain) {
                
                /*--- If iDomain owns the point, it must be either an interior
                 node (iPoint < nPointDomain) or a periodic node. ---*/
                
                if (iPoint > geometry->GetGlobal_nPointDomain() - 1)
                  iPointCurrent = iPointPeriodic;
                else
                  iPointCurrent = iPointDomain;
                
              } else {
                
                /*--- Otherwise, it must be a ghost point for iDomain ---*/
                iPointCurrent = iPointGhost;
                
              }
              
              /*--- Setting global to local, the color, and index. ---*/
              
              PointIn[iPoint] = true;
              
              Buffer_Send_Color[PointTotal_Counter+iPointCurrent] = local_colour_values[iPoint];
              Buffer_Send_GlobalPointIndex[PointTotal_Counter+iPointCurrent] = iPoint;
              
              /*--- Get the coordinates for this point ---*/
              
              for (iDim = 0; iDim < nDim_s[iDomain]; iDim++) {
                
                /*--- iPoint is the global index, but we store everything local
                 to this rank. So we need to subtract the starting index. All
                 ranks re-index their points from zero. ---*/
                Buffer_Send_Coord[nDim_s[iDomain]*(PointTotal_Counter+iPointCurrent)+iDim] = geometry->node[iPoint-geometry->starting_node[rank]]->GetCoord(iDim);
              }
              
              /*--- Increment our counters ---*/
              if ( local_colour_values[iPoint] == iDomain ) {
                if ( iPoint > geometry->GetGlobal_nPointDomain() - 1)
                  iPointPeriodic++;
                else
                  iPointDomain++;
              }
              else iPointGhost++;
              
              /*--- Increment the total number of points we're sending ---*/
              iPointTotal++;
              
            }
          }
        }
        
        /*--- Load the connectivity for the current element into the send buffer.
         Also store the local to global mapping for the elements.
         Note that we are using the vnode_local array we filled above to store
         the connectivity. Loop through each element type. ---*/
        
        switch(geometry->elem[iElem]->GetVTK_Type()) {
          case TRIANGLE:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Triangle[3*(ElemTriangle_Counter+iElemTriangle)+iNode] = vnodes_local[iNode];
            Local_to_global_Triangle[ElemTriangle_Counter+iElemTriangle] = Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemTriangle++; break;
          case QUADRILATERAL:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Quadrilateral[4*(ElemQuadrilateral_Counter+iElemQuadrilateral)+iNode] = vnodes_local[iNode];
            Local_to_global_Quadrilateral[ElemQuadrilateral_Counter+iElemQuadrilateral] =Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemQuadrilateral++; break;
          case TETRAHEDRON:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Tetrahedron[4*(ElemTetrahedron_Counter+iElemTetrahedron)+iNode] = vnodes_local[iNode];
            Local_to_global_Tetrahedron[ElemTetrahedron_Counter+iElemTetrahedron] =Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemTetrahedron++; break;
          case HEXAHEDRON:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Hexahedron[8*(ElemHexahedron_Counter+iElemHexahedron)+iNode] = vnodes_local[iNode];
            Local_to_global_Hexahedron[ElemHexahedron_Counter+iElemHexahedron] =Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemHexahedron++; break;
          case PRISM:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Prism[6*(ElemPrism_Counter+iElemPrism)+iNode] = vnodes_local[iNode];
            Local_to_global_Prism[ElemPrism_Counter+iElemPrism] =Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemPrism++; break;
          case PYRAMID:
            for (iNode = 0; iNode < geometry->elem[iElem]->GetnNodes(); iNode++)
              Buffer_Send_Pyramid[5*(ElemPyramid_Counter+iElemPyramid)+iNode] = vnodes_local[iNode];
            Local_to_global_Pyramid[ElemPyramid_Counter+iElemPyramid] = Buffer_Send_GlobElem[ElemTotal_Counter+iElemTotal];
            iElemPyramid++; break;
        }
        
        /*--- Regardless of the type, increment the total count ---*/
        iElemTotal++;
        
      }
    }
    
    /*--- Send the buffers with the geometrical information ---*/
    
    if (iDomain != (unsigned long)rank) {
      
#ifdef HAVE_MPI
      
      /*--- Communicate the coordinates, global index, colors, and element
       date to iDomain with non-blocking sends. ---*/
      
      SU2_MPI::Isend(&Buffer_Send_Coord[PointTotal_Counter*nDim_s[iDomain]],
                     nPointTotal_s[iDomain]*nDim_s[iDomain], MPI_DOUBLE, iDomain,
                     iDomain*16+0,  MPI_COMM_WORLD, &send_req[0]);
      
      SU2_MPI::Isend(&Buffer_Send_GlobalPointIndex[PointTotal_Counter],
                     nPointTotal_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+1,  MPI_COMM_WORLD, &send_req[1]);
      
      SU2_MPI::Isend(&Buffer_Send_Color[PointTotal_Counter],
                     nPointTotal_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+2,  MPI_COMM_WORLD, &send_req[2]);
      
      SU2_MPI::Isend(&Buffer_Send_Triangle[ElemTriangle_Counter*3],
                     nElemTriangle_s[iDomain]*3, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+3,  MPI_COMM_WORLD, &send_req[3]);
      
      SU2_MPI::Isend(&Buffer_Send_Quadrilateral[ElemQuadrilateral_Counter*4],
                     nElemQuadrilateral_s[iDomain]*4, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+4,  MPI_COMM_WORLD, &send_req[4]);
      
      SU2_MPI::Isend(&Buffer_Send_Tetrahedron[ElemTetrahedron_Counter*4],
                     nElemTetrahedron_s[iDomain]*4, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+5,  MPI_COMM_WORLD, &send_req[5]);
      
      SU2_MPI::Isend(&Buffer_Send_Hexahedron[ElemHexahedron_Counter*8],
                     nElemHexahedron_s[iDomain]*8, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+6,  MPI_COMM_WORLD, &send_req[6]);
      
      SU2_MPI::Isend(&Buffer_Send_Prism[ElemPrism_Counter*6],
                     nElemPrism_s[iDomain]*6, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+7,  MPI_COMM_WORLD, &send_req[7]);
      
      SU2_MPI::Isend(&Buffer_Send_Pyramid[ElemPyramid_Counter*5],
                     nElemPyramid_s[iDomain]*5, MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+8,  MPI_COMM_WORLD, &send_req[8]);
      
      SU2_MPI::Isend(&Buffer_Send_GlobElem[ElemTotal_Counter],
                     nElemTotal_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+9,  MPI_COMM_WORLD, &send_req[9]);
      
      SU2_MPI::Isend(&Local_to_global_Triangle[ElemTriangle_Counter],
                     nElemTriangle_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+10,  MPI_COMM_WORLD, &send_req[10]);
      
      SU2_MPI::Isend(&Local_to_global_Quadrilateral[ElemQuadrilateral_Counter],
                     nElemQuadrilateral_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+11,  MPI_COMM_WORLD, &send_req[11]);
      
      SU2_MPI::Isend(&Local_to_global_Tetrahedron[ElemTetrahedron_Counter],
                     nElemTetrahedron_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+12,  MPI_COMM_WORLD, &send_req[12]);
      
      SU2_MPI::Isend(&Local_to_global_Hexahedron[ElemHexahedron_Counter],
                     nElemHexahedron_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+13,  MPI_COMM_WORLD, &send_req[13]);
      
      SU2_MPI::Isend(&Local_to_global_Prism[ElemPrism_Counter],
                     nElemPrism_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+14,  MPI_COMM_WORLD, &send_req[14]);
      
      SU2_MPI::Isend(&Local_to_global_Pyramid[ElemPyramid_Counter],
                     nElemPyramid_s[iDomain], MPI_UNSIGNED_LONG, iDomain,
                     iDomain*16+15,  MPI_COMM_WORLD, &send_req[15]);
      
#endif
      
    } else {
      
      /*--- Allocate local memory for the local recv of the elements ---*/
      
      Buffer_Receive_Coord_loc  = new su2double[nPointTotal_s[iDomain]*nDim_s[iDomain]];
      
      Buffer_Receive_GlobalPointIndex_loc = new unsigned long[nPointTotal_s[iDomain]];
      Buffer_Receive_Color_loc            = new unsigned long[nPointTotal_s[iDomain]];
      Buffer_Receive_Triangle_loc         = new unsigned long[nElemTriangle_s[iDomain]*N_POINTS_TRIANGLE];
      Buffer_Receive_Quadrilateral_loc    = new unsigned long[nElemQuadrilateral_s[iDomain]*N_POINTS_QUADRILATERAL];
      Buffer_Receive_Tetrahedron_loc      = new unsigned long[nElemTetrahedron_s[iDomain]*N_POINTS_TETRAHEDRON];
      Buffer_Receive_Hexahedron_loc       = new unsigned long[nElemHexahedron_s[iDomain]*N_POINTS_HEXAHEDRON];
      Buffer_Receive_Prism_loc            = new unsigned long[nElemPrism_s[iDomain]*N_POINTS_PRISM];
      Buffer_Receive_Pyramid_loc          = new unsigned long[nElemPyramid_s[iDomain]*N_POINTS_PYRAMID];
      Buffer_Receive_GlobElem_loc         = new unsigned long[nElemTotal_s[iDomain]];
      
      Buffer_Receive_Triangle_presence_loc      = new unsigned long[nElemTriangle_s[iDomain]];
      Buffer_Receive_Quadrilateral_presence_loc = new unsigned long[nElemQuadrilateral_s[iDomain]];
      Buffer_Receive_Tetrahedron_presence_loc   = new unsigned long[nElemTetrahedron_s[iDomain]];
      Buffer_Receive_Hexahedron_presence_loc    = new unsigned long[nElemHexahedron_s[iDomain]];
      Buffer_Receive_Prism_presence_loc         = new unsigned long[nElemPrism_s[iDomain]];
      Buffer_Receive_Pyramid_presence_loc       = new unsigned long[nElemPyramid_s[iDomain]];
      
      for (iter = 0; iter < nPointTotal_s[iDomain]*nDim_s[iDomain]; iter++)
        Buffer_Receive_Coord_loc[iter] = Buffer_Send_Coord[PointTotal_Counter*nDim_s[iDomain]+iter];
      
      for (iter = 0; iter < nPointTotal_s[iDomain]; iter++) {
        Buffer_Receive_GlobalPointIndex_loc[iter] = Buffer_Send_GlobalPointIndex[PointTotal_Counter+iter];
        Buffer_Receive_Color_loc[iter] = Buffer_Send_Color[PointTotal_Counter+iter];
      }
      
      for (iter = 0; iter < nElemTriangle_s[iDomain]*N_POINTS_TRIANGLE; iter++)
        Buffer_Receive_Triangle_loc[iter] =  Buffer_Send_Triangle[ElemTriangle_Counter*N_POINTS_TRIANGLE+iter];
      
      for (iter = 0; iter < nElemQuadrilateral_s[iDomain]*N_POINTS_QUADRILATERAL; iter++)
        Buffer_Receive_Quadrilateral_loc[iter] =  Buffer_Send_Quadrilateral[ElemQuadrilateral_Counter*N_POINTS_QUADRILATERAL+iter];
      
      for (iter = 0; iter < nElemTetrahedron_s[iDomain]*N_POINTS_TETRAHEDRON; iter++)
        Buffer_Receive_Tetrahedron_loc[iter] =  Buffer_Send_Tetrahedron[ElemTetrahedron_Counter*N_POINTS_TETRAHEDRON+iter];
      
      for (iter = 0; iter < nElemHexahedron_s[iDomain]*N_POINTS_HEXAHEDRON; iter++)
        Buffer_Receive_Hexahedron_loc[iter] =  Buffer_Send_Hexahedron[ElemHexahedron_Counter*N_POINTS_HEXAHEDRON+iter];
      
      for (iter = 0; iter < nElemPrism_s[iDomain]*N_POINTS_PRISM; iter++)
        Buffer_Receive_Prism_loc[iter] =  Buffer_Send_Prism[ElemPrism_Counter*N_POINTS_PRISM+iter];
      
      for (iter = 0; iter < nElemPyramid_s[iDomain]*N_POINTS_PYRAMID; iter++)
        Buffer_Receive_Pyramid_loc[iter] =  Buffer_Send_Pyramid[ElemPyramid_Counter*N_POINTS_PYRAMID+iter];
      
      for (unsigned long i=0; i<nElemTotal_s[iDomain]; i++) {
        Buffer_Receive_GlobElem_loc[i]=Buffer_Send_GlobElem[ElemTotal_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemTriangle_s[iDomain]; i++) {
        Buffer_Receive_Triangle_presence_loc[i]=Local_to_global_Triangle[ElemTriangle_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemQuadrilateral_s[iDomain]; i++) {
        Buffer_Receive_Quadrilateral_presence_loc[i]=Local_to_global_Quadrilateral[ElemQuadrilateral_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemTetrahedron_s[iDomain]; i++) {
        Buffer_Receive_Tetrahedron_presence_loc[i]=Local_to_global_Tetrahedron[ElemTetrahedron_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemHexahedron_s[iDomain]; i++) {
        Buffer_Receive_Hexahedron_presence_loc[i]=Local_to_global_Hexahedron[ElemHexahedron_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemPrism_s[iDomain]; i++) {
        Buffer_Receive_Prism_presence_loc[i]=Local_to_global_Prism[ElemPrism_Counter+i];
      }
      
      for (unsigned long i=0; i<nElemPyramid_s[iDomain]; i++) {
        Buffer_Receive_Pyramid_presence_loc[i]=Local_to_global_Pyramid[ElemPyramid_Counter+i];
      }
    }
    
    /*--- Increment the counters for the send buffers (iDomain loop) ---*/
    
    ElemTotal_Counter         += iElemTotal;
    PointTotal_Counter        += iPointTotal;
    PointDomain_Counter       += iPointDomain;
    /*--- WARNING: check the next two counters ---*/
    PointPeriodic_Counter     += iPointPeriodic;
    PointGhost_Counter        += iPointGhost;
    ElemTriangle_Counter      += iElemTriangle;
    ElemQuadrilateral_Counter += iElemQuadrilateral;
    ElemTetrahedron_Counter   += iElemTetrahedron;
    ElemHexahedron_Counter    += iElemHexahedron;
    ElemPrism_Counter         += iElemPrism;
    ElemPyramid_Counter       += iElemPyramid;
    
  }
  
#ifdef HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  /*--- The next section begins the recv of all data for the interior
   points/elements in the mesh. First, create the domain structures for
   the points on this rank ---*/
  
  nPoint = nPointTotal_r_tot;
  nPointDomain = nPointDomainTotal_r_tot;
  nPointNode = nPoint;
  node = new CPoint*[nPoint];
  Local_to_Global_Point = new long[nPoint];
  
  /*--- Array initialization ---*/
  
  for (iPoint = 0; iPoint < nPointTotal_r_tot; iPoint++) {
    Local_to_Global_Point[iPoint] = -1;
  }
  
  /*--- Initialize some counters ---*/
  
  unsigned long temp_node_count = 0;
  unsigned long temp_node_count_periodic = nPointDomainTotal_r_tot;
  unsigned long temp_node_count_ghost = nPointDomainTotal_r_tot+nPointPeriodic_r_tot;
  
  
  /*--- First, we recv all of the point data ---*/
  
  for (iDomain = 0; iDomain < (unsigned long)size; iDomain++) {
    
    if ((unsigned long)rank != iDomain) {
      
#ifdef HAVE_MPI
      
      /*--- Allocate the receive buffer vector. Send the colors so that we
       know whether what we recv is an owned or halo node. ---*/
      
      Buffer_Receive_Coord =  new su2double [nPointTotal_r[iDomain]*nDim_r[iDomain]];
      Buffer_Receive_Color =  new unsigned long [nPointTotal_r[iDomain]];
      Buffer_Receive_GlobalPointIndex = new unsigned long [nPointTotal_r[iDomain]];
      
      /*--- Receive the buffers with the coords, global index, and colors ---*/
      
      MPI_Probe(iDomain, rank*16+0, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_DOUBLE, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Coord, recv_count , MPI_DOUBLE,
                    source, rank*16+0, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+1, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_GlobalPointIndex, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+1, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+2, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Color, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+2, MPI_COMM_WORLD, &status2);
      
      /*--- Loop over all of the points that we have recv'd and store the
       coords, global index, and colors ---*/
      
      unsigned long index=0;
      for (iPoint = 0; iPoint < nPointTotal_r[iDomain]; iPoint++) {
        
        /*--- If this rank owns the current point ---*/
        
        if (Buffer_Receive_Color[iPoint] == (unsigned long)rank) {
          
          /*--- If iDomain owns the point, it must be either an interior
           node (iPoint < nPointDomain) or a periodic node. ---*/
          
          if (Buffer_Receive_GlobalPointIndex[iPoint] > geometry->GetGlobal_nPointDomain() - 1) {
            
            /*--- Set the starting point for the local index of the recv points.
             The temp_node_count increments for the interior nodes, between 0 up
             to nPointDomain-1. ---*/
            index = temp_node_count_periodic;
            
            /*--- Get the global index ---*/
            Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex[iPoint];
            
            /*--- Allocating the Point object ---*/
            if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                      Buffer_Receive_Coord[iPoint*nDim+1],
                                                      Local_to_Global_Point[index], config);
            if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                      Buffer_Receive_Coord[iPoint*nDim+1],
                                                      Buffer_Receive_Coord[iPoint*nDim+2],
                                                      Local_to_Global_Point[index], config);
            
            /*--- Set the color ---*/
            node[index]->SetColor(Buffer_Receive_Color[iPoint]);
            
            /*--- Increment the interior node counter ---*/
            temp_node_count_periodic++;
            
            
          }
          
          else {
            
            
            /*--- Set the starting point for the local index of the recv points.
             The temp_node_count increments for the interior nodes, between 0 up
             to nPointDomain-1. ---*/
            index = temp_node_count;
            
            /*--- Get the global index ---*/
            Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex[iPoint];
            
            /*--- Allocating the Point object ---*/
            if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                      Buffer_Receive_Coord[iPoint*nDim+1],
                                                      Local_to_Global_Point[index], config);
            if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                      Buffer_Receive_Coord[iPoint*nDim+1],
                                                      Buffer_Receive_Coord[iPoint*nDim+2],
                                                      Local_to_Global_Point[index], config);
            
            /*--- Set the color ---*/
            node[index]->SetColor(Buffer_Receive_Color[iPoint]);
            
            /*--- Increment the interior node counter ---*/
            temp_node_count++;
            
            
            
            
          }
          
          
        } else {
          
          /*--- Set the starting point for the local index of the recv points.
           The temp_node_count_domain increments for the ghost nodes, between
           nPointDomain up to nPoint. ---*/
          
          index=temp_node_count_ghost;
          
          /*--- Get the global index ---*/
          Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex[iPoint];
          
          /*--- Allocating the Point object ---*/
          if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                    Buffer_Receive_Coord[iPoint*nDim+1],
                                                    Local_to_Global_Point[index], config);
          if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord[iPoint*nDim+0],
                                                    Buffer_Receive_Coord[iPoint*nDim+1],
                                                    Buffer_Receive_Coord[iPoint*nDim+2],
                                                    Local_to_Global_Point[index], config);
          
          /*--- Set the color ---*/
          node[index]->SetColor(Buffer_Receive_Color[iPoint]);
          
          /*--- Increment the ghost node counter ---*/
          temp_node_count_ghost++;
          
        }
      }
      
      /*--- Delete memory for recv the point stuff ---*/
      delete [] Buffer_Receive_Coord;
      delete [] Buffer_Receive_Color;
      delete [] Buffer_Receive_GlobalPointIndex;
      
#endif
      
    } else {
      
      /*--- Recv the point data from ourselves (same procedure as above) ---*/
      
      unsigned long index = 0;
      for (iPoint = 0; iPoint < nPointTotal_r[iDomain]; iPoint++) {
        
        if (Buffer_Receive_Color_loc[iPoint] == (unsigned long)rank) {
          
          /*--- If iDomain owns the point, it must be either an interior
           node (iPoint < nPointDomain) or a periodic node. ---*/
          
          if (Buffer_Receive_GlobalPointIndex_loc[iPoint] > geometry->GetGlobal_nPointDomain() - 1) {
            
            index = temp_node_count_periodic;
            
            Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex_loc[iPoint];
            if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                      Local_to_Global_Point[index], config);
            if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+2],
                                                      Local_to_Global_Point[index], config);
            node[index]->SetColor(Buffer_Receive_Color_loc[iPoint]);
            temp_node_count_periodic++;
            
            
            
            
          }
          else {
            
            index = temp_node_count;
            Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex_loc[iPoint];
            if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                      Local_to_Global_Point[index], config);
            if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                      Buffer_Receive_Coord_loc[iPoint*nDim+2],
                                                      Local_to_Global_Point[index], config);
            node[index]->SetColor(Buffer_Receive_Color_loc[iPoint]);
            temp_node_count++;
            
            
            
          }
          
          
        } else {
          
          index=temp_node_count_ghost;
          Local_to_Global_Point[index] = Buffer_Receive_GlobalPointIndex_loc[iPoint];
          if ( nDim == 2 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                    Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                    Local_to_Global_Point[index], config);
          if ( nDim == 3 ) node[index] = new CPoint(Buffer_Receive_Coord_loc[iPoint*nDim+0],
                                                    Buffer_Receive_Coord_loc[iPoint*nDim+1],
                                                    Buffer_Receive_Coord_loc[iPoint*nDim+2],
                                                    Local_to_Global_Point[index], config);
          node[index]->SetColor(Buffer_Receive_Color_loc[iPoint]);
          temp_node_count_ghost++;
          
        }
      }
      
      delete [] Buffer_Receive_Coord_loc;
      delete [] Buffer_Receive_Color_loc;
      delete [] Buffer_Receive_GlobalPointIndex_loc;
      
    }
  }
  
  /*--- Get the global to local mapping ---*/
  
  for (iPoint = 0; iPoint < nPointTotal_r_tot; iPoint++) {
    Global_to_local_Point_recv[Local_to_Global_Point[iPoint]] = iPoint;
  }
  
#ifdef HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  /*--- Recv all of the element data. First decide which elements we need to own on each proc ---*/
  
  iElem = 0;
  for (iDomain = 0; iDomain < (unsigned long)size; iDomain++) {
    
    if ((unsigned long)rank != iDomain) {
      
#ifdef HAVE_MPI
      
      /*--- Allocate memory for the element recv ---*/
      
      Buffer_Receive_Triangle_presence[iDomain]      = new unsigned long[nElemTriangle_r[iDomain]];
      Buffer_Receive_Quadrilateral_presence[iDomain] = new unsigned long[nElemQuadrilateral_r[iDomain]];
      Buffer_Receive_Tetrahedron_presence[iDomain]   = new unsigned long[nElemTetrahedron_r[iDomain]];
      Buffer_Receive_Hexahedron_presence[iDomain]    = new unsigned long[nElemHexahedron_r[iDomain]];
      Buffer_Receive_Prism_presence[iDomain]         = new unsigned long[nElemPrism_r[iDomain]];
      Buffer_Receive_Pyramid_presence[iDomain]       = new unsigned long[nElemPyramid_r[iDomain]];
      
      /*--- Recv the element data ---*/
      
      MPI_Probe(iDomain, rank*16+10, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Triangle_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+10, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+11, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Quadrilateral_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+11, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+12, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Tetrahedron_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+12, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+13, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Hexahedron_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+13, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+14, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Prism_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+14, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+15, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(&Buffer_Receive_Pyramid_presence[iDomain][0],
                    recv_count, MPI_UNSIGNED_LONG, source,
                    rank*16+15, MPI_COMM_WORLD, &status2);
      
      /*--- Allocating the elements after the recv ---*/
      
      for (iElemTriangle = 0; iElemTriangle < nElemTriangle_r[iDomain]; iElemTriangle++) {
        map<unsigned long, bool>::const_iterator MI = Triangle_presence.find(Buffer_Receive_Triangle_presence[iDomain][iElemTriangle]);
        if (MI == Triangle_presence.end()) {
          Triangle_presence[Buffer_Receive_Triangle_presence[iDomain][iElemTriangle]] = true;
          iElem++;
        }
      }
      
      for (iElemQuadrilateral = 0; iElemQuadrilateral < nElemQuadrilateral_r[iDomain]; iElemQuadrilateral++) {
        map<unsigned long, bool>::const_iterator MI = Quadrilateral_presence.find(Buffer_Receive_Quadrilateral_presence[iDomain][iElemQuadrilateral]);
        if (MI == Quadrilateral_presence.end()) {
          Quadrilateral_presence[Buffer_Receive_Quadrilateral_presence[iDomain][iElemQuadrilateral]] = true;
          iElem++;
        }
      }
      
      for (iElemTetrahedron = 0; iElemTetrahedron < nElemTetrahedron_r[iDomain]; iElemTetrahedron++) {
        map<unsigned long, bool>::const_iterator MI = Tetrahedron_presence.find(Buffer_Receive_Tetrahedron_presence[iDomain][iElemTetrahedron]);
        if (MI == Tetrahedron_presence.end()) {
          Tetrahedron_presence[Buffer_Receive_Tetrahedron_presence[iDomain][iElemTetrahedron]] = true;
          iElem++;
        }
      }
      
      for (iElemHexahedron = 0; iElemHexahedron < nElemHexahedron_r[iDomain]; iElemHexahedron++) {
        map<unsigned long, bool>::const_iterator MI = Hexahedron_presence.find(Buffer_Receive_Hexahedron_presence[iDomain][iElemHexahedron]);
        if (MI == Hexahedron_presence.end()) {
          Hexahedron_presence[Buffer_Receive_Hexahedron_presence[iDomain][iElemHexahedron]] = true;
          iElem++;
        }
      }
      
      for (iElemPrism = 0; iElemPrism < nElemPrism_r[iDomain]; iElemPrism++) {
        map<unsigned long, bool>::const_iterator MI = Prism_presence.find(Buffer_Receive_Prism_presence[iDomain][iElemPrism]);
        if (MI == Prism_presence.end()) {
          Prism_presence[Buffer_Receive_Prism_presence[iDomain][iElemPrism]] = true;
          iElem++;
        }
      }
      
      for (iElemPyramid = 0; iElemPyramid < nElemPyramid_r[iDomain]; iElemPyramid++) {
        map<unsigned long, bool>::const_iterator MI = Pyramid_presence.find(Buffer_Receive_Pyramid_presence[iDomain][iElemPyramid]);
        if (MI == Pyramid_presence.end()) {
          Pyramid_presence[Buffer_Receive_Pyramid_presence[iDomain][iElemPyramid]] = true;
          iElem++;
        }
      }
      
#endif
      
    } else {
      
      /*--- Store the element data from our own local rank info ---*/
      
      for (iElemTriangle = 0; iElemTriangle < nElemTriangle_r[iDomain]; iElemTriangle++) {
        map<unsigned long, bool>::const_iterator MI = Triangle_presence.find(Buffer_Receive_Triangle_presence_loc[iElemTriangle]);
        if (MI == Triangle_presence.end()) {
          Triangle_presence[Buffer_Receive_Triangle_presence_loc[iElemTriangle]] = true;
          iElem++;
        }
      }
      
      for (iElemQuadrilateral = 0; iElemQuadrilateral < nElemQuadrilateral_r[iDomain]; iElemQuadrilateral++) {
        map<unsigned long, bool>::const_iterator MI = Quadrilateral_presence.find(Buffer_Receive_Quadrilateral_presence_loc[iElemQuadrilateral]);
        if (MI == Quadrilateral_presence.end()) {
          Quadrilateral_presence[Buffer_Receive_Quadrilateral_presence_loc[iElemQuadrilateral]] = true;
          iElem++;
        }
      }
      
      for (iElemTetrahedron = 0; iElemTetrahedron < nElemTetrahedron_r[iDomain]; iElemTetrahedron++) {
        map<unsigned long, bool>::const_iterator MI = Tetrahedron_presence.find(Buffer_Receive_Tetrahedron_presence_loc[iElemTetrahedron]);
        if (MI == Tetrahedron_presence.end()) {
          Tetrahedron_presence[Buffer_Receive_Tetrahedron_presence_loc[iElemTetrahedron]] = true;
          iElem++;
        }
      }
      
      for (iElemHexahedron = 0; iElemHexahedron < nElemHexahedron_r[iDomain]; iElemHexahedron++) {
        map<unsigned long, bool>::const_iterator MI = Hexahedron_presence.find(Buffer_Receive_Hexahedron_presence_loc[iElemHexahedron]);
        if (MI == Hexahedron_presence.end()) {
          Hexahedron_presence[Buffer_Receive_Hexahedron_presence_loc[iElemHexahedron]] = true;
          iElem++;
        }
      }
      
      for (iElemPrism = 0; iElemPrism < nElemPrism_r[iDomain]; iElemPrism++) {
        map<unsigned long, bool>::const_iterator MI = Prism_presence.find(Buffer_Receive_Prism_presence_loc[iElemPrism]);
        if (MI == Prism_presence.end()) {
          Prism_presence[Buffer_Receive_Prism_presence_loc[iElemPrism]] = true;
          iElem++;
        }
      }
      
      for (iElemPyramid = 0; iElemPyramid < nElemPyramid_r[iDomain]; iElemPyramid++) {
        map<unsigned long, bool>::const_iterator MI = Pyramid_presence.find(Buffer_Receive_Pyramid_presence_loc[iElemPyramid]);
        if (MI == Pyramid_presence.end()) {
          Pyramid_presence[Buffer_Receive_Pyramid_presence_loc[iElemPyramid]] = true;
          iElem++;
        }
      }
      
    }
  }
  
#ifdef HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  /*--- iElem now contains the number of elements that this processor needs in
   total. Now we can complete the recv of the element connectivity and only
   store the elements that we need on this particular rank. Initialize space
   for the elements on this rank. ---*/
  
  nElem = iElem; iElem = 0;
  elem = new CPrimalGrid*[nElem];
  unsigned long iElemTria = 0;
  unsigned long iElemRect = 0;
  unsigned long iElemTetr = 0;
  unsigned long iElemHexa = 0;
  unsigned long iElemPris = 0;
  unsigned long iElemPyra = 0;
  
  unsigned long iElemRecv = 0;
  
  /*--- Reset presence before storing elems now that we know nElem ---*/
  
  Triangle_presence.clear();
  Quadrilateral_presence.clear();
  Tetrahedron_presence.clear();
  Hexahedron_presence.clear();
  Prism_presence.clear();
  Pyramid_presence.clear();
  
  /*--- Now recv all of the element connectivity data ---*/
  
  for (iDomain = 0; iDomain < (unsigned long)size; iDomain++) {
    
    if ((unsigned long)rank != iDomain) {
      
#ifdef HAVE_MPI
      
      /*--- Allocate memory for the element recv ---*/
      
      Buffer_Receive_Triangle      = new unsigned long[nElemTriangle_r[iDomain]*N_POINTS_TRIANGLE];
      Buffer_Receive_Quadrilateral = new unsigned long[nElemQuadrilateral_r[iDomain]*N_POINTS_QUADRILATERAL];
      Buffer_Receive_Tetrahedron   = new unsigned long[nElemTetrahedron_r[iDomain]*N_POINTS_TETRAHEDRON];
      Buffer_Receive_Hexahedron    = new unsigned long[nElemHexahedron_r[iDomain]*N_POINTS_HEXAHEDRON];
      Buffer_Receive_Prism         = new unsigned long[nElemPrism_r[iDomain]*N_POINTS_PRISM];
      Buffer_Receive_Pyramid       = new unsigned long[nElemPyramid_r[iDomain]*N_POINTS_PYRAMID];
      Buffer_Receive_GlobElem      = new unsigned long[nElemTotal_r[iDomain]];
      
      /*--- Recv the element data ---*/
      
      MPI_Probe(iDomain, rank*16+3, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Triangle, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+3, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+4, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Quadrilateral, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+4, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+5, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Tetrahedron, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+5, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+6, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Hexahedron, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+6, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+7, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Prism, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+7, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+8, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_Pyramid, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+8, MPI_COMM_WORLD, &status2);
      
      MPI_Probe(iDomain, rank*16+9, MPI_COMM_WORLD, &status2);
      source = status2.MPI_SOURCE;
      MPI_Get_count(&status2, MPI_UNSIGNED_LONG, &recv_count);
      SU2_MPI::Recv(Buffer_Receive_GlobElem, recv_count, MPI_UNSIGNED_LONG,
                    source, rank*16+9, MPI_COMM_WORLD, &status2);
      
      /*--- Allocating the elements after the recv. Note that here we are
       reusing the presence arrays to make sure that we find the exact same
       set of elements that were counted above to get nElem. ---*/
      
      iElemRecv = 0;
      
      for (iElemTriangle = 0; iElemTriangle < nElemTriangle_r[iDomain]; iElemTriangle++) {
        map<unsigned long, bool>::const_iterator MI = Triangle_presence.find(Buffer_Receive_Triangle_presence[iDomain][iElemTriangle]);
        if (MI == Triangle_presence.end()) {
          Triangle_presence[Buffer_Receive_Triangle_presence[iDomain][iElemTriangle]] = true;
          elem[iElem] = new CTriangle(Global_to_local_Point_recv[Buffer_Receive_Triangle[iElemTriangle*3+0]],
                                      Global_to_local_Point_recv[Buffer_Receive_Triangle[iElemTriangle*3+1]],
                                      Global_to_local_Point_recv[Buffer_Receive_Triangle[iElemTriangle*3+2]], 2);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemTria++;
        }
        iElemRecv++;
      }
      
      for (iElemQuadrilateral = 0; iElemQuadrilateral < nElemQuadrilateral_r[iDomain]; iElemQuadrilateral++) {
        map<unsigned long, bool>::const_iterator MI = Quadrilateral_presence.find(Buffer_Receive_Quadrilateral_presence[iDomain][iElemQuadrilateral]);
        if (MI == Quadrilateral_presence.end()) {
          Quadrilateral_presence[Buffer_Receive_Quadrilateral_presence[iDomain][iElemQuadrilateral]] = true;
          elem[iElem] = new CQuadrilateral(Global_to_local_Point_recv[Buffer_Receive_Quadrilateral[iElemQuadrilateral*4+0]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral[iElemQuadrilateral*4+1]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral[iElemQuadrilateral*4+2]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral[iElemQuadrilateral*4+3]], 2);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemRect++;
        }
        iElemRecv++;
      }
      
      for (iElemTetrahedron = 0; iElemTetrahedron < nElemTetrahedron_r[iDomain]; iElemTetrahedron++) {
        map<unsigned long, bool>::const_iterator MI = Tetrahedron_presence.find(Buffer_Receive_Tetrahedron_presence[iDomain][iElemTetrahedron]);
        if (MI == Tetrahedron_presence.end()) {
          Tetrahedron_presence[Buffer_Receive_Tetrahedron_presence[iDomain][iElemTetrahedron]] = true;
          elem[iElem] = new CTetrahedron(Global_to_local_Point_recv[Buffer_Receive_Tetrahedron[iElemTetrahedron*4+0]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron[iElemTetrahedron*4+1]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron[iElemTetrahedron*4+2]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron[iElemTetrahedron*4+3]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemTetr++;
        }
        iElemRecv++;
      }
      
      for (iElemHexahedron = 0; iElemHexahedron < nElemHexahedron_r[iDomain]; iElemHexahedron++) {
        map<unsigned long, bool>::const_iterator MI = Hexahedron_presence.find(Buffer_Receive_Hexahedron_presence[iDomain][iElemHexahedron]);
        if (MI == Hexahedron_presence.end()) {
          Hexahedron_presence[Buffer_Receive_Hexahedron_presence[iDomain][iElemHexahedron]] = true;
          elem[iElem] = new CHexahedron(Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+0]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+1]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+2]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+3]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+4]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+5]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+6]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron[iElemHexahedron*8+7]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemHexa++;
        }
        iElemRecv++;
      }
      
      for (iElemPrism = 0; iElemPrism < nElemPrism_r[iDomain]; iElemPrism++) {
        map<unsigned long, bool>::const_iterator MI = Prism_presence.find(Buffer_Receive_Prism_presence[iDomain][iElemPrism]);
        if (MI == Prism_presence.end()) {
          Prism_presence[Buffer_Receive_Prism_presence[iDomain][iElemPrism]] = true;
          elem[iElem] = new CPrism(Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+0]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+1]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+2]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+3]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+4]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism[iElemPrism*6+5]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemPris++;
        }
        iElemRecv++;
      }
      
      for (iElemPyramid = 0; iElemPyramid < nElemPyramid_r[iDomain]; iElemPyramid++) {
        map<unsigned long, bool>::const_iterator MI = Pyramid_presence.find(Buffer_Receive_Pyramid_presence[iDomain][iElemPyramid]);
        if (MI == Pyramid_presence.end()) {
          Pyramid_presence[Buffer_Receive_Pyramid_presence[iDomain][iElemPyramid]] = true;
          elem[iElem] = new CPyramid(Global_to_local_Point_recv[Buffer_Receive_Pyramid[iElemPyramid*5+0]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid[iElemPyramid*5+1]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid[iElemPyramid*5+2]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid[iElemPyramid*5+3]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid[iElemPyramid*5+4]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem[iElemRecv]);
          iElem++; iElemPyra++;
        }
        iElemRecv++;
      }
      
      /*--- Free memory for the element data --*/
      
      delete[] Buffer_Receive_Triangle;
      delete[] Buffer_Receive_Quadrilateral;
      delete[] Buffer_Receive_Tetrahedron;
      delete[] Buffer_Receive_Hexahedron;
      delete[] Buffer_Receive_Prism;
      delete[] Buffer_Receive_Pyramid;
      delete[] Buffer_Receive_GlobElem;
      
      delete[] Buffer_Receive_Triangle_presence[iDomain];
      delete[] Buffer_Receive_Quadrilateral_presence[iDomain];
      delete[] Buffer_Receive_Tetrahedron_presence[iDomain];
      delete[] Buffer_Receive_Hexahedron_presence[iDomain];
      delete[] Buffer_Receive_Prism_presence[iDomain];
      delete[] Buffer_Receive_Pyramid_presence[iDomain];
      
#endif
      
    } else {
      
      /*--- Store the element data from our local rank ---*/
      
      iElemRecv = 0;
      
      for (iElemTriangle = 0; iElemTriangle < nElemTriangle_r[iDomain]; iElemTriangle++) {
        map<unsigned long, bool>::const_iterator MI = Triangle_presence.find(Buffer_Receive_Triangle_presence_loc[iElemTriangle]);
        if (MI == Triangle_presence.end()) {
          Triangle_presence[Buffer_Receive_Triangle_presence_loc[iElemTriangle]] = true;
          elem[iElem] = new CTriangle(Global_to_local_Point_recv[Buffer_Receive_Triangle_loc[iElemTriangle*3+0]],
                                      Global_to_local_Point_recv[Buffer_Receive_Triangle_loc[iElemTriangle*3+1]],
                                      Global_to_local_Point_recv[Buffer_Receive_Triangle_loc[iElemTriangle*3+2]], 2);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemTria++;
        }
        iElemRecv++;
      }
      
      for (iElemQuadrilateral = 0; iElemQuadrilateral < nElemQuadrilateral_r[iDomain]; iElemQuadrilateral++) {
        map<unsigned long, bool>::const_iterator MI = Quadrilateral_presence.find(Buffer_Receive_Quadrilateral_presence_loc[iElemQuadrilateral]);
        if (MI == Quadrilateral_presence.end()) {
          Quadrilateral_presence[Buffer_Receive_Quadrilateral_presence_loc[iElemQuadrilateral]] = true;
          elem[iElem] = new CQuadrilateral(Global_to_local_Point_recv[Buffer_Receive_Quadrilateral_loc[iElemQuadrilateral*4+0]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral_loc[iElemQuadrilateral*4+1]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral_loc[iElemQuadrilateral*4+2]],
                                           Global_to_local_Point_recv[Buffer_Receive_Quadrilateral_loc[iElemQuadrilateral*4+3]], 2);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemRect++;
        }
        iElemRecv++;
      }
      
      for (iElemTetrahedron = 0; iElemTetrahedron < nElemTetrahedron_r[iDomain]; iElemTetrahedron++) {
        map<unsigned long, bool>::const_iterator MI = Tetrahedron_presence.find(Buffer_Receive_Tetrahedron_presence_loc[iElemTetrahedron]);
        if (MI == Tetrahedron_presence.end()) {
          Tetrahedron_presence[Buffer_Receive_Tetrahedron_presence_loc[iElemTetrahedron]] = true;
          elem[iElem] = new CTetrahedron(Global_to_local_Point_recv[Buffer_Receive_Tetrahedron_loc[iElemTetrahedron*4+0]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron_loc[iElemTetrahedron*4+1]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron_loc[iElemTetrahedron*4+2]],
                                         Global_to_local_Point_recv[Buffer_Receive_Tetrahedron_loc[iElemTetrahedron*4+3]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemTetr++;
        }
        iElemRecv++;
      }
      
      for (iElemHexahedron = 0; iElemHexahedron < nElemHexahedron_r[iDomain]; iElemHexahedron++) {
        map<unsigned long, bool>::const_iterator MI = Hexahedron_presence.find(Buffer_Receive_Hexahedron_presence_loc[iElemHexahedron]);
        if (MI == Hexahedron_presence.end()) {
          Hexahedron_presence[Buffer_Receive_Hexahedron_presence_loc[iElemHexahedron]] = true;
          elem[iElem] = new CHexahedron(Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+0]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+1]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+2]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+3]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+4]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+5]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+6]],
                                        Global_to_local_Point_recv[Buffer_Receive_Hexahedron_loc[iElemHexahedron*8+7]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemHexa++;
        }
        iElemRecv++;
      }
      
      for (iElemPrism = 0; iElemPrism < nElemPrism_r[iDomain]; iElemPrism++) {
        map<unsigned long, bool>::const_iterator MI = Prism_presence.find(Buffer_Receive_Prism_presence_loc[iElemPrism]);
        if (MI == Prism_presence.end()) {
          Prism_presence[Buffer_Receive_Prism_presence_loc[iElemPrism]] = true;
          elem[iElem] = new CPrism(Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+0]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+1]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+2]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+3]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+4]],
                                   Global_to_local_Point_recv[Buffer_Receive_Prism_loc[iElemPrism*6+5]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemPris++;
        }
        iElemRecv++;
      }
      
      for (iElemPyramid = 0; iElemPyramid < nElemPyramid_r[iDomain]; iElemPyramid++) {
        map<unsigned long, bool>::const_iterator MI = Pyramid_presence.find(Buffer_Receive_Pyramid_presence_loc[iElemPyramid]);
        if (MI == Pyramid_presence.end()) {
          Pyramid_presence[Buffer_Receive_Pyramid_presence_loc[iElemPyramid]] = true;
          elem[iElem] = new CPyramid(Global_to_local_Point_recv[Buffer_Receive_Pyramid_loc[iElemPyramid*5+0]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid_loc[iElemPyramid*5+1]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid_loc[iElemPyramid*5+2]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid_loc[iElemPyramid*5+3]],
                                     Global_to_local_Point_recv[Buffer_Receive_Pyramid_loc[iElemPyramid*5+4]]);
          elem[iElem]->SetGlobalIndex(Buffer_Receive_GlobElem_loc[iElemRecv]);
          iElem++; iElemPyra++;
        }
        iElemRecv++;
      }
      
      /*--- Free memory for element data ---*/
      
      delete[] Buffer_Receive_Triangle_loc;
      delete[] Buffer_Receive_Quadrilateral_loc;
      delete[] Buffer_Receive_Tetrahedron_loc;
      delete[] Buffer_Receive_Hexahedron_loc;
      delete[] Buffer_Receive_Prism_loc;
      delete[] Buffer_Receive_Pyramid_loc;
      delete[] Buffer_Receive_GlobElem_loc;
      
      delete[] Buffer_Receive_Triangle_presence_loc;
      delete[] Buffer_Receive_Quadrilateral_presence_loc;
      delete[] Buffer_Receive_Tetrahedron_presence_loc;
      delete[] Buffer_Receive_Hexahedron_presence_loc;
      delete[] Buffer_Receive_Prism_presence_loc;
      delete[] Buffer_Receive_Pyramid_presence_loc;
      
    }
  }
  
#ifdef HAVE_MPI
  for (iDomain = 0; iDomain < (unsigned long)size; iDomain++) {
    if ((unsigned long)rank != iDomain) SU2_MPI::Waitall(16, send_req, send_stat);
  }
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  /*--- Free all of the memory used for communicating points and elements ---*/
  
  delete [] Buffer_Send_Coord;
  delete [] Buffer_Send_GlobalPointIndex;
  delete [] Buffer_Send_Color;
  delete [] Buffer_Send_Triangle;
  delete [] Buffer_Send_Quadrilateral;
  delete [] Buffer_Send_Tetrahedron;
  delete [] Buffer_Send_Hexahedron;
  delete [] Buffer_Send_Prism;
  delete [] Buffer_Send_Pyramid;
  delete [] Buffer_Send_GlobElem;
  delete [] Buffer_Send_BoundLine;
  delete [] Buffer_Send_BoundTriangle;
  delete [] Buffer_Send_BoundQuadrilateral;
  delete [] Buffer_Send_Local2Global_Marker;
  
  delete [] Buffer_Send_SendDomain_Periodic;
  delete [] Buffer_Send_SendDomain_PeriodicTrans;
  delete [] Buffer_Send_SendDomain_PeriodicReceptor;
  delete [] Buffer_Send_ReceivedDomain_Periodic;
  delete [] Buffer_Send_ReceivedDomain_PeriodicTrans;
  delete [] Buffer_Send_ReceivedDomain_PeriodicDonor;
  
#ifdef HAVE_MPI
  delete [] Buffer_Receive_Triangle_presence;
  delete [] Buffer_Receive_Quadrilateral_presence;
  delete [] Buffer_Receive_Tetrahedron_presence;
  delete [] Buffer_Receive_Hexahedron_presence;
  delete [] Buffer_Receive_Prism_presence;
  delete [] Buffer_Receive_Pyramid_presence;
#endif
  
  delete [] Local_to_global_Triangle;
  delete [] Local_to_global_Quadrilateral;
  delete [] Local_to_global_Tetrahedron;
  delete [] Local_to_global_Hexahedron;
  delete [] Local_to_global_Prism;
  delete [] Local_to_global_Pyramid;
  
  
  /*--- Communicate the number of each element type to all processors. These
   values are important for merging and writing output later. ---*/
  
#ifdef HAVE_MPI
  unsigned long Local_nElem = nElem;
  SU2_MPI::Allreduce(&Local_nElem, &Global_nElem, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nElem = nElem;
#endif
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
    cout << Global_nElem << " interior elements including halo cells. " << endl;
  
  /*--- Set the value of Global_nElemDomain (stored in the geometry container that is passed in) ---*/

  Global_nElemDomain = geometry->GetGlobal_nElemDomain();

  /*--- Store total number of each element type after incrementing the
   counters in the recv loop above (to make sure there aren't repeats). ---*/
  
  nelem_triangle = iElemTria;
  nelem_quad     = iElemRect;
  nelem_tetra    = iElemTetr;
  nelem_hexa     = iElemHexa;
  nelem_prism    = iElemPris;
  nelem_pyramid  = iElemPyra;
  
#ifdef HAVE_MPI
  unsigned long Local_nElemTri     = nelem_triangle;
  unsigned long Local_nElemQuad    = nelem_quad;
  unsigned long Local_nElemTet     = nelem_tetra;
  unsigned long Local_nElemHex     = nelem_hexa;
  unsigned long Local_nElemPrism   = nelem_prism;
  unsigned long Local_nElemPyramid = nelem_pyramid;
  SU2_MPI::Allreduce(&Local_nElemTri, &Global_nelem_triangle, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemQuad, &Global_nelem_quad, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemTet, &Global_nelem_tetra, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemHex, &Global_nelem_hexa, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemPrism, &Global_nelem_prism, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemPyramid, &Global_nelem_pyramid, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nelem_triangle = nelem_triangle;
  Global_nelem_quad     = nelem_quad;
  Global_nelem_tetra    = nelem_tetra;
  Global_nelem_hexa     = nelem_hexa;
  Global_nelem_prism    = nelem_prism;
  Global_nelem_pyramid  = nelem_pyramid;
#endif
  
  /*--- Print information about the elements to the console ---*/
  
  if (rank == MASTER_NODE) {
    if (Global_nelem_triangle > 0)  cout << Global_nelem_triangle << " triangles."      << endl;
    if (Global_nelem_quad > 0)      cout << Global_nelem_quad     << " quadrilaterals." << endl;
    if (Global_nelem_tetra > 0)     cout << Global_nelem_tetra    << " tetrahedra."     << endl;
    if (Global_nelem_hexa > 0)      cout << Global_nelem_hexa     << " hexahedra."      << endl;
    if (Global_nelem_prism > 0)     cout << Global_nelem_prism    << " prisms."         << endl;
    if (Global_nelem_pyramid > 0)   cout << Global_nelem_pyramid  << " pyramids."       << endl;
  }
  
  /*--- Now partition the boundary elements on the markers. Note that, for
   now, we are still performing the boundary partitioning using the master
   node alone. The boundaries should make up a much smaller portion of the
   mesh, so this is ok for now, but we will transition to a parallel version
   of this soon that follows the same procedure above for the interior. ---*/
  
  if (rank == MASTER_NODE) {
    
    /*--- Create auxiliary vectors based on the original geometry ---*/
    
    MarkerIn = new bool[geometry->GetnMarker()];
    VertexIn = new bool*[geometry->GetnMarker()];
    
    for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++)
      VertexIn[iMarker] = new bool[geometry->GetnElem_Bound(iMarker)];
    
    Buffer_Send_nDim      = geometry->GetnDim();
    Buffer_Send_nZone     = geometry->GetnZone();
    Buffer_Send_nPeriodic = config->GetnPeriodicIndex();
    Buffer_Send_Center    = new su2double[Buffer_Send_nPeriodic*3];
    Buffer_Send_Rotation  = new su2double[Buffer_Send_nPeriodic*3];
    Buffer_Send_Translate = new su2double[Buffer_Send_nPeriodic*3];
    
    Buffer_Send_nSendDomain_Periodic     = new unsigned long[nDomain];
    Buffer_Send_nReceivedDomain_Periodic = new unsigned long[nDomain];
    
    /*--- Create a local copy of config->GetMarker_All_SendRecv and
     config->GetMarker_All_TagBound in the master node ---*/
    
    Marker_All_SendRecv_Copy = new short[geometry->GetnMarker()];
    Marker_All_TagBound_Copy = new string[geometry->GetnMarker()];
    
    for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
      Marker_All_SendRecv_Copy[iMarker] = config->GetMarker_All_SendRecv(iMarker);
      Marker_All_TagBound_Copy[iMarker] = config->GetMarker_All_TagBound(iMarker);
    }
    
  }
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    if (rank == MASTER_NODE) {
      
      /*--- Interior dimensionalization. Loop over the original grid
       to perform the dimensionalizaton of the domain variables ---*/
      
      //      Buffer_Send_nElemTotal         = 0;
      //      Buffer_Send_nPointTotal        = 0;
      //      Buffer_Send_nPointGhost        = 0;
      //      Buffer_Send_nPointDomainTotal  = 0;
      //      Buffer_Send_nPointPeriodic     = 0;
      //      Buffer_Send_nElemTriangle      = 0;
      //      Buffer_Send_nElemQuadrilateral = 0;
      //      Buffer_Send_nElemTetrahedron   = 0;
      //      Buffer_Send_nElemHexahedron    = 0;
      //      Buffer_Send_nElemPrism         = 0;
      //      Buffer_Send_nElemPyramid       = 0;
      
      /*--- Boundary dimensionalization. Dimensionalization with physical
       boundaries, compute Buffer_Send_nMarkerDomain,
       Buffer_Send_nVertexDomain[nMarkerDomain] ---*/
      
      Buffer_Send_nMarkerDomain        = 0;
      Buffer_Send_nBoundLineTotal      = 0;
      Buffer_Send_nBoundTriangleTotal  = 0;
      Buffer_Send_nBoundQuadrilateralTotal = 0;
      
      for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
        Buffer_Send_nVertexDomain[iMarker]   = 0;
        Buffer_Send_nBoundLine[iMarker]      = 0;
        Buffer_Send_nBoundTriangle[iMarker]  = 0;
        Buffer_Send_nBoundQuadrilateral[iMarker] = 0;
        Buffer_Send_Marker_All_SendRecv[iMarker] = Marker_All_SendRecv_Copy[iMarker];
        SPRINTF(&Buffer_Send_Marker_All_TagBound[iMarker*MAX_STRING_SIZE], "%s",
                Marker_All_TagBound_Copy[iMarker].c_str());
      }
      
      for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
        if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) {
          
          MarkerIn[iMarker] = false;
          Buffer_Send_nVertexDomain[Buffer_Send_nMarkerDomain] = 0;
          
          for (iVertex = 0; iVertex < geometry->GetnElem_Bound(iMarker); iVertex++) {
            VertexIn[iMarker][iVertex] = false;
            for (iNode = 0; iNode < geometry->bound[iMarker][iVertex]->GetnNodes(); iNode++) {
              iPoint = geometry->bound[iMarker][iVertex]->GetNode(iNode);
              if (local_colour_values[iPoint] == iDomain) VertexIn[iMarker][iVertex] = true;
            }
            
            /*--- If this vertex should be sent, increment the element type ---*/
            if (VertexIn[iMarker][iVertex]) {
              switch(geometry->bound[iMarker][iVertex]->GetVTK_Type()) {
                case LINE:
                  Buffer_Send_nBoundLine[Buffer_Send_nMarkerDomain]++;
                  Buffer_Send_nBoundLineTotal++;
                  break;
                case TRIANGLE:
                  Buffer_Send_nBoundTriangle[Buffer_Send_nMarkerDomain]++;
                  Buffer_Send_nBoundTriangleTotal++;
                  break;
                case QUADRILATERAL:
                  Buffer_Send_nBoundQuadrilateral[Buffer_Send_nMarkerDomain]++;
                  Buffer_Send_nBoundQuadrilateralTotal++;
                  break;
              }
              
              /*--- Increment the total number of vertices to be sent ---*/
              Buffer_Send_nVertexDomain[Buffer_Send_nMarkerDomain]++;
              MarkerIn[iMarker] = true;
              
            }
          }
          
          /*--- Increment the number of markers to be sent ---*/
          if (MarkerIn[iMarker]) { Buffer_Send_nMarkerDomain++; }
          
        }
      }
      
      /*--- Copy periodic information from the config file ---*/
      
      for (iPeriodic = 0; iPeriodic < Buffer_Send_nPeriodic; iPeriodic++) {
        for (iDim = 0; iDim < 3; iDim++) {
          Buffer_Send_Center[iDim+iPeriodic*3]    = config->GetPeriodicCenter(iPeriodic)[iDim];
          Buffer_Send_Rotation[iDim+iPeriodic*3]  = config->GetPeriodicRotation(iPeriodic)[iDim];
          Buffer_Send_Translate[iDim+iPeriodic*3] = config->GetPeriodicTranslate(iPeriodic)[iDim];
        }
      }
      
      /*--- Dimensionalization of the periodic auxiliary vectors ---*/
      
      for (jDomain = 0; jDomain < nDomain; jDomain++) {
        Buffer_Send_nSendDomain_Periodic[jDomain]     = 0;
        Buffer_Send_nReceivedDomain_Periodic[jDomain] = 0;
      }
      Buffer_Send_nTotalSendDomain_Periodic     = 0;
      Buffer_Send_nTotalReceivedDomain_Periodic = 0;
      
      for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
        if (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) {
          for (iVertex = 0; iVertex < geometry->GetnElem_Bound(iMarker); iVertex++) {
            iPoint = geometry->bound[iMarker][iVertex]->GetNode(0);
            if (iDomain == local_colour_values[iPoint]) {
              
              if (config->GetMarker_All_SendRecv(iMarker) > 0) {
                
                /*--- Identify the color of the receptor ---*/
                
                for (jMarker = 0; jMarker < geometry->GetnMarker(); jMarker++) {
                  if ((config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) &&
                      (config->GetMarker_All_SendRecv(jMarker) == -config->GetMarker_All_SendRecv(iMarker))) {
                    jPoint = geometry->bound[jMarker][iVertex]->GetNode(0);
                    ReceptorColor = local_colour_values[jPoint];
                  }
                }
                
                Buffer_Send_nSendDomain_Periodic[ReceptorColor]++;
                Buffer_Send_nTotalSendDomain_Periodic++;
                
              }
              if (config->GetMarker_All_SendRecv(iMarker) < 0) {
                
                /*--- Identify the color of the donor ---*/
                
                for (jMarker = 0; jMarker < geometry->GetnMarker(); jMarker++) {
                  if ((config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) &&
                      (config->GetMarker_All_SendRecv(jMarker) == -config->GetMarker_All_SendRecv(iMarker))) {
                    jPoint = geometry->bound[jMarker][iVertex]->GetNode(0);
                    DonorColor = local_colour_values[jPoint];
                  }
                }
                
                Buffer_Send_nReceivedDomain_Periodic[DonorColor]++;
                Buffer_Send_nTotalReceivedDomain_Periodic++;
                
              }
            }
          }
        }
      }
      
      /*--- Allocate the buffer vectors in the appropiate domain (master, iDomain) ---*/
      
      Buffer_Send_BoundLine           = new unsigned long[Buffer_Send_nBoundLineTotal*N_POINTS_LINE];
      Buffer_Send_BoundTriangle       = new unsigned long[Buffer_Send_nBoundTriangleTotal*N_POINTS_TRIANGLE];
      Buffer_Send_BoundQuadrilateral  = new unsigned long[Buffer_Send_nBoundQuadrilateralTotal*N_POINTS_QUADRILATERAL];
      Buffer_Send_Local2Global_Marker = new unsigned long[Buffer_Send_nMarkerDomain];
      
      Buffer_Send_SendDomain_Periodic           = new unsigned long[Buffer_Send_nTotalSendDomain_Periodic];
      Buffer_Send_SendDomain_PeriodicTrans      = new unsigned long[Buffer_Send_nTotalSendDomain_Periodic];
      Buffer_Send_SendDomain_PeriodicReceptor   = new unsigned long[Buffer_Send_nTotalSendDomain_Periodic];
      Buffer_Send_ReceivedDomain_Periodic       = new unsigned long[Buffer_Send_nTotalReceivedDomain_Periodic];
      Buffer_Send_ReceivedDomain_PeriodicTrans  = new unsigned long[Buffer_Send_nTotalReceivedDomain_Periodic];
      Buffer_Send_ReceivedDomain_PeriodicDonor  = new unsigned long[Buffer_Send_nTotalReceivedDomain_Periodic];
      
      if (iDomain != (unsigned long)MASTER_NODE) {
        
#ifdef HAVE_MPI
        
        SU2_MPI::Isend(&Buffer_Send_nBoundLineTotal, 1,
                       MPI_UNSIGNED_LONG, iDomain,
                       0, MPI_COMM_WORLD, &send_req[0]);
        
        SU2_MPI::Isend(&Buffer_Send_nBoundTriangleTotal, 1,
                       MPI_UNSIGNED_LONG, iDomain,
                       1, MPI_COMM_WORLD, &send_req[1]);
        
        SU2_MPI::Isend(&Buffer_Send_nBoundQuadrilateralTotal, 1,
                       MPI_UNSIGNED_LONG,  iDomain,
                       2, MPI_COMM_WORLD, &send_req[2]);
        
        SU2_MPI::Isend(&Buffer_Send_nMarkerDomain, 1,
                       MPI_UNSIGNED_SHORT, iDomain,
                       3, MPI_COMM_WORLD, &send_req[3]);
        
        SU2_MPI::Isend(Buffer_Send_nVertexDomain,
                       nMarker_Max, MPI_UNSIGNED_LONG, iDomain,
                       4, MPI_COMM_WORLD, &send_req[4]);
        
        SU2_MPI::Isend(Buffer_Send_nBoundLine,
                       nMarker_Max, MPI_UNSIGNED_LONG, iDomain,
                       5, MPI_COMM_WORLD, &send_req[5]);
        
        SU2_MPI::Isend(Buffer_Send_nBoundTriangle,
                       nMarker_Max, MPI_UNSIGNED_LONG, iDomain,
                       6, MPI_COMM_WORLD, &send_req[6]);
        
        SU2_MPI::Isend(Buffer_Send_nBoundQuadrilateral,
                       nMarker_Max, MPI_UNSIGNED_LONG, iDomain,
                       7, MPI_COMM_WORLD, &send_req[7]);
        
        SU2_MPI::Isend(Buffer_Send_Marker_All_SendRecv,
                       nMarker_Max, MPI_SHORT, iDomain,
                       8, MPI_COMM_WORLD, &send_req[8]);
        
        SU2_MPI::Isend(Buffer_Send_Marker_All_TagBound,
                       nMarker_Max*MAX_STRING_SIZE, MPI_CHAR, iDomain,
                       9, MPI_COMM_WORLD, &send_req[9]);
        
        SU2_MPI::Isend(&Buffer_Send_nPeriodic,
                       1, MPI_UNSIGNED_SHORT, iDomain,
                       10, MPI_COMM_WORLD, &send_req[10]);
        
        SU2_MPI::Isend(Buffer_Send_Center,
                       nPeriodic*3, MPI_DOUBLE, iDomain,
                       11, MPI_COMM_WORLD, &send_req[11]);
        
        SU2_MPI::Isend(Buffer_Send_Rotation,
                       nPeriodic*3, MPI_DOUBLE, iDomain,
                       12, MPI_COMM_WORLD, &send_req[12]);
        
        SU2_MPI::Isend(Buffer_Send_Translate,
                       nPeriodic*3, MPI_DOUBLE, iDomain,
                       13, MPI_COMM_WORLD, &send_req[13]);
        
        SU2_MPI::Isend(&Buffer_Send_nTotalSendDomain_Periodic,
                       1, MPI_UNSIGNED_LONG, iDomain,
                       14, MPI_COMM_WORLD, &send_req[14]);
        
        SU2_MPI::Isend(&Buffer_Send_nTotalReceivedDomain_Periodic,
                       1, MPI_UNSIGNED_LONG, iDomain,
                       15, MPI_COMM_WORLD, &send_req[15]);
        
        SU2_MPI::Isend(Buffer_Send_nSendDomain_Periodic,
                       nDomain, MPI_UNSIGNED_LONG, iDomain,
                       16, MPI_COMM_WORLD, &send_req[16]);
        
        SU2_MPI::Isend(Buffer_Send_nReceivedDomain_Periodic,
                       nDomain, MPI_UNSIGNED_LONG, iDomain,
                       17, MPI_COMM_WORLD, &send_req[17]);
        
        /*--- Wait for this set of non-blocking comm. to complete ---*/
        
        SU2_MPI::Waitall(18, send_req, send_stat);
        
#endif
        
      } else {
        
        /*--- We are the master node, so simply copy values into place ---*/
        
        nDim  = Buffer_Send_nDim;
        nZone = Buffer_Send_nZone;
        
        nPeriodic      = Buffer_Send_nPeriodic;
        //        nPointGhost    = Buffer_Send_nPointGhost;
        //        nPointPeriodic = Buffer_Send_nPointPeriodic;
        
        nBoundLineTotal      = Buffer_Send_nBoundLineTotal;
        nBoundTriangleTotal  = Buffer_Send_nBoundTriangleTotal;
        nBoundQuadrilateralTotal = Buffer_Send_nBoundQuadrilateralTotal;
        nMarkerDomain        = Buffer_Send_nMarkerDomain;
        
        for (iMarker = 0; iMarker < nMarker_Max; iMarker++) {
          nVertexDomain[iMarker] = Buffer_Send_nVertexDomain[iMarker];
          nBoundLine[iMarker] = Buffer_Send_nBoundLine[iMarker];
          nBoundTriangle[iMarker] = Buffer_Send_nBoundTriangle[iMarker];
          nBoundQuadrilateral[iMarker] = Buffer_Send_nBoundQuadrilateral[iMarker];
          Marker_All_SendRecv[iMarker] = Buffer_Send_Marker_All_SendRecv[iMarker];
          for (iter = 0; iter < MAX_STRING_SIZE; iter++)
            Marker_All_TagBound[iMarker*MAX_STRING_SIZE+iter] = Buffer_Send_Marker_All_TagBound[iMarker*MAX_STRING_SIZE+iter];
        }
        
        Buffer_Receive_Center    = new su2double[nPeriodic*3];
        Buffer_Receive_Rotation  = new su2double[nPeriodic*3];
        Buffer_Receive_Translate = new su2double[nPeriodic*3];
        
        for (iter = 0; iter < nPeriodic*3; iter++) {
          Buffer_Receive_Center[iter]    =  Buffer_Send_Center[iter];
          Buffer_Receive_Rotation[iter]  =  Buffer_Send_Rotation[iter];
          Buffer_Receive_Translate[iter] =  Buffer_Send_Translate[iter];
        }
        
        nTotalSendDomain_Periodic     = Buffer_Send_nTotalSendDomain_Periodic;
        nTotalReceivedDomain_Periodic = Buffer_Send_nTotalReceivedDomain_Periodic;
        
        for (iter = 0; iter < nDomain; iter++) {
          nSendDomain_Periodic[iter] = Buffer_Send_nSendDomain_Periodic[iter];
          nReceivedDomain_Periodic[iter] = Buffer_Send_nReceivedDomain_Periodic[iter];
        }
        
      }
    }
    
    /*--- Each rank now begins to receive information from the master ---*/
    
    if ((unsigned long)rank == iDomain) {
      
      /*--- First, receive the size of buffers before receiving the data ---*/
      
      if (rank != MASTER_NODE) {
        
#ifdef HAVE_MPI
        
        MPI_Probe(MASTER_NODE, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(&nBoundLineTotal, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 0, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 1, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(&nBoundTriangleTotal, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 1, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 2, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(&nBoundQuadrilateralTotal, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 2, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 3, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_SHORT, &recv_count);
        SU2_MPI::Recv(&nMarkerDomain, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 3, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 4, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nVertexDomain, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 4, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 5, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nBoundLine, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 5, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 6, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nBoundTriangle, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 6, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 7, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nBoundQuadrilateral, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 7, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 8, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_SHORT, &recv_count);
        SU2_MPI::Recv(Marker_All_SendRecv, recv_count, MPI_SHORT,
                      MASTER_NODE, 8, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 9, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CHAR, &recv_count);
        SU2_MPI::Recv(Marker_All_TagBound, recv_count, MPI_CHAR,
                      MASTER_NODE, 9, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 10, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_SHORT, &recv_count);
        SU2_MPI::Recv(&nPeriodic, recv_count, MPI_UNSIGNED_SHORT,
                      MASTER_NODE, 10, MPI_COMM_WORLD, &status);
        
#endif
        
        /*--- Marker_All_TagBound and Marker_All_SendRecv, set the same
         values in the config files of all the files ---*/
        
        for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
          config->SetMarker_All_SendRecv(iMarker,
                                         Marker_All_SendRecv[iMarker]);
          config->SetMarker_All_TagBound(iMarker,
                                         string(&Marker_All_TagBound[iMarker*MAX_STRING_SIZE]));
        }
        
        
        /*--- Periodic boundary conditions ---*/
        
        Buffer_Receive_Center    = new su2double[nPeriodic*3];
        Buffer_Receive_Rotation  = new su2double[nPeriodic*3];
        Buffer_Receive_Translate = new su2double[nPeriodic*3];
        
#ifdef HAVE_MPI
        
        MPI_Probe(MASTER_NODE, 11, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_DOUBLE, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_Center, recv_count, MPI_DOUBLE,
                      MASTER_NODE, 11, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 12, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_DOUBLE, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_Rotation, recv_count, MPI_DOUBLE,
                      MASTER_NODE, 12, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 13, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_DOUBLE, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_Translate, recv_count, MPI_DOUBLE,
                      MASTER_NODE, 13, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 14, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(&nTotalSendDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 14, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 15, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(&nTotalReceivedDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 15, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 16, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nSendDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 16, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 17, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(nReceivedDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 17, MPI_COMM_WORLD, &status);
        
#endif
        
        config->SetnPeriodicIndex(nPeriodic);
        
        for (iPeriodic = 0; iPeriodic < nPeriodic; iPeriodic++) {
          
          su2double* center    = new su2double[3];
          su2double* rotation  = new su2double[3];
          su2double* translate = new su2double[3];
          
          for (iDim = 0; iDim < 3; iDim++) {
            center[iDim] = Buffer_Receive_Center[iDim+iPeriodic*3];
            rotation[iDim] = Buffer_Receive_Rotation[iDim+iPeriodic*3];
            translate[iDim] = Buffer_Receive_Translate[iDim+iPeriodic*3];
          }
          config->SetPeriodicCenter(iPeriodic, center);
          config->SetPeriodicRotation(iPeriodic, rotation);
          config->SetPeriodicTranslate(iPeriodic, translate);
        
          delete [] center; delete [] rotation; delete [] translate;
          
        }
        
      }
      
      delete [] Buffer_Receive_Center;
      delete [] Buffer_Receive_Rotation;
      delete [] Buffer_Receive_Translate;
      
      /*--- Allocate the receive buffer vector ---*/
      
      Buffer_Receive_BoundLine           = new unsigned long[nBoundLineTotal*2];
      Buffer_Receive_BoundTriangle       = new unsigned long[nBoundTriangleTotal*3];
      Buffer_Receive_BoundQuadrilateral      = new unsigned long[nBoundQuadrilateralTotal*4];
      Buffer_Receive_Local2Global_Marker = new unsigned long[nMarkerDomain];
      
      Buffer_Receive_SendDomain_Periodic          = new unsigned long[nTotalSendDomain_Periodic];
      Buffer_Receive_SendDomain_PeriodicTrans     = new unsigned long[nTotalSendDomain_Periodic];
      Buffer_Receive_SendDomain_PeriodicReceptor  = new unsigned long[nTotalSendDomain_Periodic];
      Buffer_Receive_ReceivedDomain_Periodic      = new unsigned long[nTotalReceivedDomain_Periodic];
      Buffer_Receive_ReceivedDomain_PeriodicTrans = new unsigned long[nTotalReceivedDomain_Periodic];
      Buffer_Receive_ReceivedDomain_PeriodicDonor = new unsigned long[nTotalReceivedDomain_Periodic];
      
    }
    
    /*--- Set the value of the Send buffers ---*/
    
    if (rank == MASTER_NODE) {
      
      /*--- Set the value of the boundary geometry ---*/
      
      iMarkerDomain = 0;
      iBoundLineTotal = 0; iBoundTriangleTotal = 0; iBoundQuadrilateralTotal = 0;
      
      for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
        if ((config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE) && (MarkerIn[iMarker])) {
          for (iVertex = 0; iVertex < geometry->GetnElem_Bound(iMarker); iVertex++) {
            
            if (VertexIn[iMarker][iVertex]) {
              
              /*--- Send global index here and then convert to local on the recv ---*/
              
              for (iNode = 0; iNode < geometry->bound[iMarker][iVertex]->GetnNodes(); iNode++) {
                vnodes_local[iNode] = geometry->bound[iMarker][iVertex]->GetNode(iNode);
              }
              
              switch(geometry->bound[iMarker][iVertex]->GetVTK_Type()) {
                case LINE:
                  Buffer_Send_BoundLine[N_POINTS_LINE*iBoundLineTotal+0] = vnodes_local[0];
                  Buffer_Send_BoundLine[N_POINTS_LINE*iBoundLineTotal+1] = vnodes_local[1];
                  iBoundLineTotal++;
                  break;
                case TRIANGLE:
                  Buffer_Send_BoundTriangle[N_POINTS_TRIANGLE*iBoundTriangleTotal+0] = vnodes_local[0];
                  Buffer_Send_BoundTriangle[N_POINTS_TRIANGLE*iBoundTriangleTotal+1] = vnodes_local[1];
                  Buffer_Send_BoundTriangle[N_POINTS_TRIANGLE*iBoundTriangleTotal+2] = vnodes_local[2];
                  iBoundTriangleTotal++;
                  break;
                case QUADRILATERAL:
                  Buffer_Send_BoundQuadrilateral[N_POINTS_QUADRILATERAL*iBoundQuadrilateralTotal+0] = vnodes_local[0];
                  Buffer_Send_BoundQuadrilateral[N_POINTS_QUADRILATERAL*iBoundQuadrilateralTotal+1] = vnodes_local[1];
                  Buffer_Send_BoundQuadrilateral[N_POINTS_QUADRILATERAL*iBoundQuadrilateralTotal+2] = vnodes_local[2];
                  Buffer_Send_BoundQuadrilateral[N_POINTS_QUADRILATERAL*iBoundQuadrilateralTotal+3] = vnodes_local[3];
                  iBoundQuadrilateralTotal++;
                  break;
              }
            }
          }
          
          Buffer_Send_Local2Global_Marker[iMarkerDomain] = iMarker;
          iMarkerDomain++;
          
        }
      }
      
      /*--- Evaluate the number of already existing periodic boundary conditions ---*/
      
      iTotalSendDomain_Periodic = 0;
      iTotalReceivedDomain_Periodic = 0;
      
      for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
        
        if (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) {
          
          for (iVertex = 0; iVertex < geometry->GetnElem_Bound(iMarker); iVertex++) {
            
            iPoint = geometry->bound[iMarker][iVertex]->GetNode(0);
            Transformation = geometry->bound[iMarker][iVertex]->GetRotation_Type();
            
            if (iDomain == local_colour_values[iPoint]) {
              
              /*--- If the information is going to be sended, find the
               domain of the receptor ---*/
              
              if (config->GetMarker_All_SendRecv(iMarker) > 0) {
                
                /*--- Identify the color of the receptor ---*/
                
                for (jMarker = 0; jMarker < geometry->GetnMarker(); jMarker++) {
                  if ((config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) &&
                      (config->GetMarker_All_SendRecv(jMarker) == -config->GetMarker_All_SendRecv(iMarker))) {
                    jPoint = geometry->bound[jMarker][iVertex]->GetNode(0);
                    ReceptorColor = local_colour_values[jPoint];
                  }
                }
                
                /*--- For each color of the receptor we will han an extra marker (+) ---*/
                
                Buffer_Send_SendDomain_Periodic[iTotalSendDomain_Periodic] = iPoint;
                Buffer_Send_SendDomain_PeriodicTrans[iTotalSendDomain_Periodic] = Transformation;
                Buffer_Send_SendDomain_PeriodicReceptor[iTotalSendDomain_Periodic] = ReceptorColor;
                
                iTotalSendDomain_Periodic++;
                
              }
              
              /*--- If the information is goint to be received, find the domain if the donor ---*/
              
              if (config->GetMarker_All_SendRecv(iMarker) < 0) {
                
                /*--- Identify the color of the donor ---*/
                
                for (jMarker = 0; jMarker < geometry->GetnMarker(); jMarker++) {
                  if ((config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) &&
                      (config->GetMarker_All_SendRecv(jMarker) == -config->GetMarker_All_SendRecv(iMarker) )) {
                    jPoint = geometry->bound[jMarker][iVertex]->GetNode(0);
                    DonorColor = local_colour_values[jPoint];
                  }
                }
                
                /*--- For each color of the donor we will han an extra marker (-) ---*/
                
                Buffer_Send_ReceivedDomain_Periodic[iTotalReceivedDomain_Periodic] = iPoint;
                Buffer_Send_ReceivedDomain_PeriodicTrans[iTotalReceivedDomain_Periodic] = Transformation;
                Buffer_Send_ReceivedDomain_PeriodicDonor[iTotalReceivedDomain_Periodic] = DonorColor;
                
                iTotalReceivedDomain_Periodic++;
                
              }
            }
          }
        }
      }
      
      /*--- Send the buffers with the geometrical information ---*/
      
      if (iDomain != (unsigned long)MASTER_NODE) {
        
#ifdef HAVE_MPI
        
        SU2_MPI::Isend(Buffer_Send_BoundLine,
                       Buffer_Send_nBoundLineTotal*N_POINTS_LINE, MPI_UNSIGNED_LONG, iDomain,
                       0, MPI_COMM_WORLD, &send_req[0]);
        
        SU2_MPI::Isend(Buffer_Send_BoundTriangle,
                       Buffer_Send_nBoundTriangleTotal*N_POINTS_TRIANGLE, MPI_UNSIGNED_LONG, iDomain,
                       1, MPI_COMM_WORLD, &send_req[1]);
        
        SU2_MPI::Isend(Buffer_Send_BoundQuadrilateral,
                       Buffer_Send_nBoundQuadrilateralTotal*N_POINTS_QUADRILATERAL, MPI_UNSIGNED_LONG, iDomain,
                       2, MPI_COMM_WORLD, &send_req[2]);
        
        SU2_MPI::Isend(Buffer_Send_Local2Global_Marker,
                       Buffer_Send_nMarkerDomain, MPI_UNSIGNED_LONG, iDomain,
                       3, MPI_COMM_WORLD, &send_req[3]);
        
        SU2_MPI::Isend(Buffer_Send_SendDomain_Periodic,
                       Buffer_Send_nTotalSendDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       4, MPI_COMM_WORLD, &send_req[4]);
        
        SU2_MPI::Isend(Buffer_Send_SendDomain_PeriodicTrans,
                       Buffer_Send_nTotalSendDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       5, MPI_COMM_WORLD, &send_req[5]);
        
        SU2_MPI::Isend(Buffer_Send_SendDomain_PeriodicReceptor,
                       Buffer_Send_nTotalSendDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       6, MPI_COMM_WORLD, &send_req[6]);
        
        SU2_MPI::Isend(Buffer_Send_ReceivedDomain_Periodic,
                       Buffer_Send_nTotalReceivedDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       7, MPI_COMM_WORLD, &send_req[7]);
        
        SU2_MPI::Isend(Buffer_Send_ReceivedDomain_PeriodicTrans,
                       Buffer_Send_nTotalReceivedDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       8, MPI_COMM_WORLD, &send_req[8]);
        
        SU2_MPI::Isend(Buffer_Send_ReceivedDomain_PeriodicDonor,
                       Buffer_Send_nTotalReceivedDomain_Periodic, MPI_UNSIGNED_LONG, iDomain,
                       9, MPI_COMM_WORLD, &send_req[9]);
        
        /*--- Wait for this set of non-blocking comm. to complete ---*/
        
        SU2_MPI::Waitall(10, send_req, send_stat);
        
#endif
        
      } else {
        
        /*--- Copy the data directly from our own rank ---*/
        
        for (iter = 0; iter < Buffer_Send_nBoundLineTotal*N_POINTS_LINE; iter++)
          Buffer_Receive_BoundLine[iter] =  Buffer_Send_BoundLine[iter];
        
        for (iter = 0; iter < Buffer_Send_nBoundTriangleTotal*N_POINTS_TRIANGLE; iter++)
          Buffer_Receive_BoundTriangle[iter] =  Buffer_Send_BoundTriangle[iter];
        
        for (iter = 0; iter < Buffer_Send_nBoundQuadrilateralTotal*N_POINTS_QUADRILATERAL; iter++)
          Buffer_Receive_BoundQuadrilateral[iter] =  Buffer_Send_BoundQuadrilateral[iter];
        
        for (iter = 0; iter < Buffer_Send_nMarkerDomain; iter++)
          Buffer_Receive_Local2Global_Marker[iter] =  Buffer_Send_Local2Global_Marker[iter];
        
        for (iter = 0; iter < Buffer_Send_nTotalSendDomain_Periodic; iter++) {
          Buffer_Receive_SendDomain_Periodic[iter] = Buffer_Send_SendDomain_Periodic[iter];
          Buffer_Receive_SendDomain_PeriodicTrans[iter] = Buffer_Send_SendDomain_PeriodicTrans[iter];
          Buffer_Receive_SendDomain_PeriodicReceptor[iter] = Buffer_Send_SendDomain_PeriodicReceptor[iter];
        }
        
        for (iter = 0; iter < Buffer_Send_nTotalReceivedDomain_Periodic; iter++) {
          Buffer_Receive_ReceivedDomain_Periodic[iter] = Buffer_Send_ReceivedDomain_Periodic[iter];
          Buffer_Receive_ReceivedDomain_PeriodicTrans[iter] = Buffer_Send_ReceivedDomain_PeriodicTrans[iter];
          Buffer_Receive_ReceivedDomain_PeriodicDonor[iter] = Buffer_Send_ReceivedDomain_PeriodicDonor[iter];
        }
        
      }
      
      delete[] Buffer_Send_BoundLine;
      delete[] Buffer_Send_BoundTriangle;
      delete[] Buffer_Send_BoundQuadrilateral;
      delete[] Buffer_Send_Local2Global_Marker;
      
      delete[] Buffer_Send_SendDomain_Periodic;
      delete[] Buffer_Send_SendDomain_PeriodicTrans;
      delete[] Buffer_Send_SendDomain_PeriodicReceptor;
      delete[] Buffer_Send_ReceivedDomain_Periodic;
      delete[] Buffer_Send_ReceivedDomain_PeriodicTrans;
      delete[] Buffer_Send_ReceivedDomain_PeriodicDonor;
      
    }
    
    if ((unsigned long)rank == iDomain) {
      
      if (rank != MASTER_NODE) {
        
        /*--- Receive the buffers with the geometrical information ---*/
        
#ifdef HAVE_MPI
        
        MPI_Probe(MASTER_NODE, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_BoundLine, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 0, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 1, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_BoundTriangle, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 1, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 2, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_BoundQuadrilateral, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 2, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 3, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_Local2Global_Marker, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 3, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 4, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_SendDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 4, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 5, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_SendDomain_PeriodicTrans, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 5, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 6, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_SendDomain_PeriodicReceptor, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 6, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 7, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_ReceivedDomain_Periodic, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 7, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 8, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_ReceivedDomain_PeriodicTrans, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 8, MPI_COMM_WORLD, &status);
        
        MPI_Probe(MASTER_NODE, 9, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_LONG, &recv_count);
        SU2_MPI::Recv(Buffer_Receive_ReceivedDomain_PeriodicDonor, recv_count, MPI_UNSIGNED_LONG,
                      MASTER_NODE, 9, MPI_COMM_WORLD, &status);
        
#endif
        
      }
      
      /*--- Create the domain structures for the boundaries ---*/
      
      nMarker                = nMarkerDomain;
      nElem_Bound            = new unsigned long[nMarker_Max];
      Local_to_Global_Marker = new unsigned short[nMarker_Max];
      Tag_to_Marker          = new string[nMarker_Max];
      string *TagBound_Copy  = new string[nMarker_Max];
      short *SendRecv_Copy   = new short[nMarker_Max];
      
      for (iMarker = 0; iMarker < nMarker; iMarker++)
        nElem_Bound[iMarker] = nVertexDomain[iMarker];
      
      bound = new CPrimalGrid**[nMarker+(OVERHEAD*size)];
      for (iMarker = 0; iMarker < nMarker+(OVERHEAD*size); iMarker++)
        bound[iMarker] = NULL;
    
      for (iMarker = 0; iMarker < nMarker; iMarker++)
        bound[iMarker] = new CPrimalGrid*[nElem_Bound[iMarker]];
      
      /*--- Initialize boundary element counters ---*/
      iBoundLineTotal      = 0;
      iBoundTriangleTotal  = 0;
      iBoundQuadrilateralTotal = 0;
      
      /*--- Store the boundary element connectivity. Note here that we have
       communicated the global index values for the elements, so we need to
       convert this to the local index when instantiating the element. ---*/
      
      for (iMarker = 0; iMarker < nMarker; iMarker++) {
        
        iVertexDomain = 0;
        
        for (iBoundLine = 0; iBoundLine < nBoundLine[iMarker]; iBoundLine++) {
          bound[iMarker][iVertexDomain] = new CLine(Global_to_local_Point_recv[Buffer_Receive_BoundLine[iBoundLineTotal*2+0]],
                                                    Global_to_local_Point_recv[Buffer_Receive_BoundLine[iBoundLineTotal*2+1]], 2);
          iVertexDomain++; iBoundLineTotal++;
        }
        for (iBoundTriangle = 0; iBoundTriangle < nBoundTriangle[iMarker]; iBoundTriangle++) {
          bound[iMarker][iVertexDomain] = new CTriangle(Global_to_local_Point_recv[Buffer_Receive_BoundTriangle[iBoundTriangleTotal*3+0]],
                                                        Global_to_local_Point_recv[Buffer_Receive_BoundTriangle[iBoundTriangleTotal*3+1]],
                                                        Global_to_local_Point_recv[Buffer_Receive_BoundTriangle[iBoundTriangleTotal*3+2]], 3);
          iVertexDomain++; iBoundTriangleTotal++;
        }
        for (iBoundQuadrilateral = 0; iBoundQuadrilateral < nBoundQuadrilateral[iMarker]; iBoundQuadrilateral++) {
          bound[iMarker][iVertexDomain] = new CQuadrilateral(Global_to_local_Point_recv[Buffer_Receive_BoundQuadrilateral[iBoundQuadrilateralTotal*4+0]],
                                                             Global_to_local_Point_recv[Buffer_Receive_BoundQuadrilateral[iBoundQuadrilateralTotal*4+1]],
                                                             Global_to_local_Point_recv[Buffer_Receive_BoundQuadrilateral[iBoundQuadrilateralTotal*4+2]],
                                                             Global_to_local_Point_recv[Buffer_Receive_BoundQuadrilateral[iBoundQuadrilateralTotal*4+3]], 3);
          iVertexDomain++; iBoundQuadrilateralTotal++;
        }
        
        Local_to_Global_Marker[iMarker] = Buffer_Receive_Local2Global_Marker[iMarker];
        
        /*--- Now each domain has the right information ---*/
        
        string Grid_Marker = config->GetMarker_All_TagBound(Local_to_Global_Marker[iMarker]);
        short SendRecv = config->GetMarker_All_SendRecv(Local_to_Global_Marker[iMarker]);
        TagBound_Copy[iMarker] = Grid_Marker;
        SendRecv_Copy[iMarker] = SendRecv;
        
      }
      
      /*--- Store total number of each boundary element type ---*/
      
      nelem_edge_bound     = iBoundLineTotal;
      nelem_triangle_bound = iBoundTriangleTotal;
      nelem_quad_bound     = iBoundQuadrilateralTotal;
      
      for (iMarker = 0; iMarker < nMarker; iMarker++) {
        config->SetMarker_All_TagBound(iMarker, TagBound_Copy[iMarker]);
        config->SetMarker_All_SendRecv(iMarker, SendRecv_Copy[iMarker]);
      }
      
      /*--- Add the new periodic markers to the domain ---*/
      
      //      iTotalSendDomain_Periodic = 0;
      //      iTotalReceivedDomain_Periodic = 0;
      
      for (jDomain = 0; jDomain < nDomain; jDomain++) {
        
        if (nSendDomain_Periodic[jDomain] != 0) {
          nVertexDomain[nMarker] = 0;
          bound[nMarker] = new CPrimalGrid* [nSendDomain_Periodic[jDomain]];
          
          iVertex = 0;
          for (iTotalSendDomain_Periodic = 0; iTotalSendDomain_Periodic < nTotalSendDomain_Periodic; iTotalSendDomain_Periodic++) {
            if (Buffer_Receive_SendDomain_PeriodicReceptor[iTotalSendDomain_Periodic] == jDomain) {
              bound[nMarker][iVertex] = new CVertexMPI(Global_to_local_Point_recv[Buffer_Receive_SendDomain_Periodic[iTotalSendDomain_Periodic]], nDim);
              bound[nMarker][iVertex]->SetRotation_Type(Buffer_Receive_SendDomain_PeriodicTrans[iTotalSendDomain_Periodic]);
              nVertexDomain[nMarker]++; iVertex++;
            }
          }
          
          Marker_All_SendRecv[nMarker] = jDomain+1;
          nElem_Bound[nMarker] = nVertexDomain[nMarker];
          nMarker++;
        }
        
        if (nReceivedDomain_Periodic[jDomain] != 0) {
          nVertexDomain[nMarker] = 0;
          bound[nMarker] = new CPrimalGrid* [nReceivedDomain_Periodic[jDomain]];
          
          iVertex = 0;
          for (iTotalReceivedDomain_Periodic = 0; iTotalReceivedDomain_Periodic < nTotalReceivedDomain_Periodic; iTotalReceivedDomain_Periodic++) {
            if (Buffer_Receive_ReceivedDomain_PeriodicDonor[iTotalReceivedDomain_Periodic] == jDomain) {
              bound[nMarker][iVertex] = new CVertexMPI(Global_to_local_Point_recv[Buffer_Receive_ReceivedDomain_Periodic[iTotalReceivedDomain_Periodic]], nDim);
              bound[nMarker][iVertex]->SetRotation_Type(Buffer_Receive_ReceivedDomain_PeriodicTrans[iTotalReceivedDomain_Periodic]);
              nVertexDomain[nMarker]++; iVertex++;
            }
          }
          
          Marker_All_SendRecv[nMarker] = -(jDomain+1);
          nElem_Bound[nMarker] = nVertexDomain[nMarker];
          nMarker++;
        }
        
      }
      
      delete[] TagBound_Copy;
      delete[] SendRecv_Copy;
      
      delete[] Buffer_Receive_BoundLine;
      delete[] Buffer_Receive_BoundTriangle;
      delete[] Buffer_Receive_BoundQuadrilateral;
      delete[] Buffer_Receive_Local2Global_Marker;
      
      delete[] Buffer_Receive_SendDomain_Periodic;
      delete[] Buffer_Receive_SendDomain_PeriodicTrans;
      delete[] Buffer_Receive_SendDomain_PeriodicReceptor;
      delete[] Buffer_Receive_ReceivedDomain_Periodic;
      delete[] Buffer_Receive_ReceivedDomain_PeriodicTrans;
      delete[] Buffer_Receive_ReceivedDomain_PeriodicDonor;
      
    }
    
  }
  
  /*--- The MASTER should wait for the sends above to complete ---*/
  
#ifdef HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
  
  /*--- Set the value of Marker_All_SendRecv and Marker_All_TagBound in the config structure ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    config->SetMarker_All_SendRecv(iMarker, Marker_All_SendRecv[iMarker]);
  }
  
  /*--- Set the value of Global_nPoint and Global_nPointDomain ---*/
  
  unsigned long Local_nPoint = nPoint;
  unsigned long Local_nPointDomain = nPointDomain;
  
#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&Local_nPoint, &Global_nPoint, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nPointDomain, &Global_nPointDomain, 1,
                     MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nPoint = Local_nPoint;
  Global_nPointDomain = Local_nPointDomain;
#endif
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
    cout << Global_nPoint << " vertices including ghost points. " << endl;
  

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
      config->SetMarker_All_SendRecv(iMarker, Marker_All_SendRecv[iMarker]);
    }

  /*--- initialize pointers for turbomachinery computations  ---*/
  nSpanWiseSections       = new unsigned short[2];
  SpanWiseValue           = new su2double*[2];
  for (iMarker = 0; iMarker < 2; iMarker++){
    nSpanWiseSections[iMarker]      = 0;
    SpanWiseValue[iMarker]          = NULL;
  }

  nVertexSpan                       = new long* [nMarker];
  nTotVertexSpan                    = new unsigned long* [nMarker];
  turbovertex                       = new CTurboVertex***[nMarker];
  AverageTurboNormal                = new su2double**[nMarker];
  AverageNormal                     = new su2double**[nMarker];
  AverageGridVel                    = new su2double**[nMarker];
  AverageTangGridVel                = new su2double*[nMarker];
  SpanArea                          = new su2double*[nMarker];
  TurboRadius                       = new su2double*[nMarker];
  MaxAngularCoord                   = new su2double*[nMarker];
  MinAngularCoord                   = new su2double*[nMarker];
  MinRelAngularCoord                = new su2double*[nMarker];

  for (iMarker = 0; iMarker < nMarker; iMarker++){
    nVertexSpan[iMarker]            = NULL;
    nTotVertexSpan[iMarker]         = NULL;
    turbovertex[iMarker]            = NULL;
    AverageTurboNormal[iMarker]     = NULL;
    AverageNormal[iMarker]          = NULL;
    AverageGridVel[iMarker]         = NULL;
    AverageTangGridVel[iMarker]     = NULL;
    SpanArea[iMarker]               = NULL;
    TurboRadius[iMarker]            = NULL;
    MaxAngularCoord[iMarker]        = NULL;
    MinAngularCoord[iMarker]        = NULL;
    MinRelAngularCoord[iMarker]     = NULL;
  }

  /*--- initialize pointers for turbomachinery performance computation  ---*/
  nTurboPerf            = config->GetnMarker_TurboPerformance();
  TangGridVelIn  		= new su2double*[config->GetnMarker_TurboPerformance()];
  SpanAreaIn 			= new su2double*[config->GetnMarker_TurboPerformance()];
  TurboRadiusIn 		= new su2double*[config->GetnMarker_TurboPerformance()];
  TangGridVelOut  		= new su2double*[config->GetnMarker_TurboPerformance()];
  SpanAreaOut 			= new su2double*[config->GetnMarker_TurboPerformance()];
  TurboRadiusOut 		= new su2double*[config->GetnMarker_TurboPerformance()];

  for (iMarker = 0; iMarker < config->GetnMarker_TurboPerformance(); iMarker++){
    TangGridVelIn[iMarker]		= NULL;
    SpanAreaIn[iMarker]			= NULL;
    TurboRadiusIn[iMarker]		= NULL;
    TangGridVelOut[iMarker]		= NULL;
    SpanAreaOut[iMarker]		= NULL;
    TurboRadiusOut[iMarker]		= NULL;
  }

  /*--- Release all of the temporary memory ---*/
  
  delete [] nDim_s;
  delete [] nDim_r;
  
  delete [] nPointTotal_s;
  delete [] nPointDomainTotal_s;
  delete [] nPointGhost_s;
  delete [] nPointPeriodic_s;
  delete [] nElemTotal_s;
  delete [] nElemTriangle_s;
  delete [] nElemQuadrilateral_s;
  delete [] nElemTetrahedron_s;
  delete [] nElemHexahedron_s;
  delete [] nElemPrism_s;
  delete [] nElemPyramid_s;
  delete [] nZone_s;
  
  delete [] nPointTotal_r;
  delete [] nPointDomainTotal_r;
  delete [] nPointGhost_r;
  delete [] nPointPeriodic_r;
  delete [] nElemTotal_r;
  delete [] nElemTriangle_r;
  delete [] nElemQuadrilateral_r;
  delete [] nElemTetrahedron_r;
  delete [] nElemHexahedron_r;
  delete [] nElemPrism_r;
  delete [] nElemPyramid_r;
  delete [] nZone_r;
  
  if (rank == MASTER_NODE) {
    delete [] MarkerIn;
    delete [] Buffer_Send_Center;
    delete [] Buffer_Send_Rotation;
    delete [] Buffer_Send_Translate;
    delete [] Buffer_Send_nSendDomain_Periodic;
    delete [] Buffer_Send_nReceivedDomain_Periodic;
    delete [] Marker_All_SendRecv_Copy;
    delete [] Marker_All_TagBound_Copy;
    for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++)
      delete [] VertexIn[iMarker];
    delete[] VertexIn;
  }
  
  delete [] Marker_All_TagBound;
  delete [] Buffer_Send_Marker_All_TagBound;
  
  delete [] nSendDomain_Periodic;
  delete [] nReceivedDomain_Periodic;
  delete [] nVertexDomain;
  delete [] nBoundLine;
  delete [] nBoundTriangle;
  delete [] nBoundQuadrilateral;
  delete [] Buffer_Send_nVertexDomain;
  delete [] Buffer_Send_nBoundLine;
  delete [] Buffer_Send_nBoundTriangle;
  delete [] Buffer_Send_nBoundQuadrilateral;
  delete [] Buffer_Send_Marker_All_SendRecv;
  
#ifdef HAVE_MPI
  delete [] send_stat;
  delete [] recv_stat;
  delete [] send_req;
  delete [] recv_req;
#endif
  
  delete [] local_colour_values;
  delete [] ElemIn;
  
}


CPhysicalGeometry::~CPhysicalGeometry(void) {
  
  if (Local_to_Global_Point  != NULL) delete [] Local_to_Global_Point;
  if (Global_to_Local_Marker != NULL) delete [] Global_to_Local_Marker;
  if (Local_to_Global_Marker != NULL) delete [] Local_to_Global_Marker;
  
}



void CPhysicalGeometry::SetSendReceive(CConfig *config) {
  
  unsigned short Counter_Send, Counter_Receive, iMarkerSend, iMarkerReceive;
  unsigned long iVertex, LocalNode;
  unsigned short nMarker_Max = config->GetnMarker_Max();
  unsigned long  iPoint, jPoint, iElem;
  unsigned long *nVertexDomain = new unsigned long[nMarker_Max];
  unsigned short nDomain, iNode, iDomain, jDomain, jNode;
  vector<unsigned long>::iterator it;
  
  vector<vector<unsigned long> > SendTransfLocal;	/*!< \brief Vector to store the type of transformation for this send point. */
  vector<vector<unsigned long> > ReceivedTransfLocal;	/*!< \brief Vector to store the type of transformation for this received point. */
	vector<vector<unsigned long> > SendDomainLocal; /*!< \brief SendDomain[from domain][to domain] and return the point index of the node that must me sended. */
	vector<vector<unsigned long> > ReceivedDomainLocal; /*!< \brief SendDomain[from domain][to domain] and return the point index of the node that must me sended. */
  
  int rank = MASTER_NODE;
  int size = SINGLE_NODE;
  
#ifdef HAVE_MPI
  /*--- MPI initialization ---*/
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  if (rank == MASTER_NODE && size > SINGLE_NODE)
    cout << "Establishing MPI communication patterns." << endl;

  nDomain = size;
  
  SendTransfLocal.resize(nDomain);
  ReceivedTransfLocal.resize(nDomain);
  SendDomainLocal.resize(nDomain);
  ReceivedDomainLocal.resize(nDomain);
  
  /*--- Loop over the all the points of the element
   to find the points with different colours, and create the send/received list ---*/
  for (iElem = 0; iElem < nElem; iElem++) {
    for (iNode = 0; iNode < elem[iElem]->GetnNodes(); iNode++) {
      iPoint = elem[iElem]->GetNode(iNode);
      iDomain = node[iPoint]->GetColor();
      
      if (iDomain == rank) {
        for (jNode = 0; jNode < elem[iElem]->GetnNodes(); jNode++) {
          jPoint = elem[iElem]->GetNode(jNode);
          jDomain = node[jPoint]->GetColor();
          
          /*--- If different color and connected by an edge, then we add them to the list ---*/
          if (iDomain != jDomain) {
            
            /*--- We send from iDomain to jDomain the value of iPoint, we save the
             global value becuase we need to sort the lists ---*/
            SendDomainLocal[jDomain].push_back(Local_to_Global_Point[iPoint]);
            /*--- We send from jDomain to iDomain the value of jPoint, we save the
             global value becuase we need to sort the lists ---*/
            ReceivedDomainLocal[jDomain].push_back(Local_to_Global_Point[jPoint]);
            
          }
        }
      }
    }
  }
  
  /*--- Sort the points that must be sended and delete repeated points, note
   that the sorting should be done with the global point (not the local) ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    sort( SendDomainLocal[iDomain].begin(), SendDomainLocal[iDomain].end());
    it = unique( SendDomainLocal[iDomain].begin(), SendDomainLocal[iDomain].end());
    SendDomainLocal[iDomain].resize( it - SendDomainLocal[iDomain].begin() );
  }
  
  /*--- Sort the points that must be received and delete repeated points, note
   that the sorting should be done with the global point (not the local) ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    sort( ReceivedDomainLocal[iDomain].begin(), ReceivedDomainLocal[iDomain].end());
    it = unique( ReceivedDomainLocal[iDomain].begin(), ReceivedDomainLocal[iDomain].end());
    ReceivedDomainLocal[iDomain].resize( it - ReceivedDomainLocal[iDomain].begin() );
  }
  
  /*--- Create Global to Local Point array, note that the array is smaller (Max_GlobalPoint) than the total
   number of points in the simulation  ---*/
  Max_GlobalPoint = 0;
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    if (Local_to_Global_Point[iPoint] > (long)Max_GlobalPoint)
      Max_GlobalPoint = Local_to_Global_Point[iPoint];
  }
  
  /*--- Set the value of some of the points ---*/
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    Global_to_Local_Point[Local_to_Global_Point[iPoint]] = iPoint;
  
  map<unsigned long, unsigned long>::const_iterator MI;

  /*--- Add the new MPI send receive boundaries, reset the transformation, and save the local value ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    if (SendDomainLocal[iDomain].size() != 0) {
      nVertexDomain[nMarker] = SendDomainLocal[iDomain].size();
      for (iVertex = 0; iVertex < nVertexDomain[nMarker]; iVertex++) {
        
        MI = Global_to_Local_Point.find(SendDomainLocal[iDomain][iVertex]);
        if (MI != Global_to_Local_Point.end()) iPoint = Global_to_Local_Point[SendDomainLocal[iDomain][iVertex]];
        else iPoint = -1;
          
        SendDomainLocal[iDomain][iVertex] = iPoint;
        SendTransfLocal[iDomain].push_back(0);
      }
      nElem_Bound[nMarker] = nVertexDomain[nMarker];
      bound[nMarker] = new CPrimalGrid*[nElem_Bound[nMarker]];
      nMarker++;
    }
  }
  
  /*--- Add the new MPI receive boundaries, reset the transformation, and save the local value ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    if (ReceivedDomainLocal[iDomain].size() != 0) {
      nVertexDomain[nMarker] = ReceivedDomainLocal[iDomain].size();
      for (iVertex = 0; iVertex < nVertexDomain[nMarker]; iVertex++) {
        
        MI = Global_to_Local_Point.find(ReceivedDomainLocal[iDomain][iVertex]);
        if (MI != Global_to_Local_Point.end()) iPoint = Global_to_Local_Point[ReceivedDomainLocal[iDomain][iVertex]];
        else iPoint = -1;
        
        ReceivedDomainLocal[iDomain][iVertex] = iPoint;
        ReceivedTransfLocal[iDomain].push_back(0);
      }
      nElem_Bound[nMarker] = nVertexDomain[nMarker];
      bound[nMarker] = new CPrimalGrid*[nElem_Bound[nMarker]];
      nMarker++;
    }
  }
  
  /*--- First compute the Send/Receive boundaries ---*/
  Counter_Send = 0; 	Counter_Receive = 0;
  for (iDomain = 0; iDomain < nDomain; iDomain++)
    if (SendDomainLocal[iDomain].size() != 0) Counter_Send++;
  
  for (iDomain = 0; iDomain < nDomain; iDomain++)
    if (ReceivedDomainLocal[iDomain].size() != 0) Counter_Receive++;
  
  iMarkerSend = nMarker - Counter_Send - Counter_Receive;
  iMarkerReceive = nMarker - Counter_Receive;
  
  /*--- First we do the send ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    if (SendDomainLocal[iDomain].size() != 0) {
      for (iVertex = 0; iVertex < GetnElem_Bound(iMarkerSend); iVertex++) {
        LocalNode = SendDomainLocal[iDomain][iVertex];
        bound[iMarkerSend][iVertex] = new CVertexMPI(LocalNode, nDim);
        bound[iMarkerSend][iVertex]->SetRotation_Type(SendTransfLocal[iDomain][iVertex]);
      }
      Marker_All_SendRecv[iMarkerSend] = iDomain+1;
      iMarkerSend++;
    }
  }
  
  /*--- Second we do the receive ---*/
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    if (ReceivedDomainLocal[iDomain].size() != 0) {
      for (iVertex = 0; iVertex < GetnElem_Bound(iMarkerReceive); iVertex++) {
        LocalNode = ReceivedDomainLocal[iDomain][iVertex];
        bound[iMarkerReceive][iVertex] = new CVertexMPI(LocalNode, nDim);
        bound[iMarkerReceive][iVertex]->SetRotation_Type(ReceivedTransfLocal[iDomain][iVertex]);
      }
      Marker_All_SendRecv[iMarkerReceive] = -(iDomain+1);
      iMarkerReceive++;
    }
  }
 
  /*--- Free memory ---*/
  delete [] nVertexDomain;
}


void CPhysicalGeometry::SetBoundaries(CConfig *config) {
  
  unsigned long iElem_Bound, TotalElem, *nElem_Bound_Copy, iVertex_;
  string Grid_Marker;
  unsigned short iDomain, nDomain, iMarkersDomain, iLoop, *DomainCount, nMarker_Physical, Duplicate_SendReceive, *DomainSendCount, **DomainSendMarkers, *DomainReceiveCount, **DomainReceiveMarkers, nMarker_SendRecv, iMarker, iMarker_;
  CPrimalGrid*** bound_Copy;
  short *Marker_All_SendRecv_Copy;
  bool CheckStart;
  
  int size = SINGLE_NODE;
  
#ifdef HAVE_MPI
  /*--- MPI initialization ---*/
  MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif
  
  nDomain = size+1;
  
  /*--- Count the number of physical markers
   in the boundaries ---*/
  
  nMarker_Physical = 0;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (bound[iMarker][0]->GetVTK_Type() != VERTEX) {
      nMarker_Physical++;
    }
  }
  
  /*--- Identify if there are markers that send/received with the same domain,
   they should be together---*/
  
  Duplicate_SendReceive = 0;
  for (iLoop = 0; iLoop < 2; iLoop++) {
    
    DomainCount = new unsigned short [nDomain];
    
    for (iDomain = 0; iDomain < nDomain; iDomain++)
      DomainCount[iDomain] = 0;
    
    if (iLoop == 0) {
      for (iDomain = 0; iDomain < nDomain; iDomain++)
        for (iMarker = 0; iMarker < nMarker; iMarker++)
          if (bound[iMarker][0]->GetVTK_Type() == VERTEX)
            if (Marker_All_SendRecv[iMarker] == iDomain) DomainCount[iDomain]++;
    }
    else {
      for (iDomain = 0; iDomain < nDomain; iDomain++)
        for (iMarker = 0; iMarker < nMarker; iMarker++)
          if (bound[iMarker][0]->GetVTK_Type() == VERTEX)
            if (Marker_All_SendRecv[iMarker] == -iDomain) DomainCount[iDomain]++;
    }
    
    for (iDomain = 0; iDomain < nDomain; iDomain++)
      if (DomainCount[iDomain] > 1) Duplicate_SendReceive++;
    
    delete [] DomainCount;
    
  }
  
  DomainSendCount = new unsigned short [nDomain];
  DomainSendMarkers = new unsigned short *[nDomain];
  DomainReceiveCount = new unsigned short [nDomain];
  DomainReceiveMarkers = new unsigned short *[nDomain];
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    DomainSendCount[iDomain] = 0;
    DomainSendMarkers[iDomain] = new unsigned short [nMarker];
    
    DomainReceiveCount[iDomain] = 0;
    DomainReceiveMarkers[iDomain] = new unsigned short [nMarker];
  }
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    for (iMarker = 0; iMarker < nMarker; iMarker++) {
      if (bound[iMarker][0]->GetVTK_Type() == VERTEX) {
        if (Marker_All_SendRecv[iMarker] == iDomain) {
          DomainSendMarkers[iDomain][DomainSendCount[iDomain]] = iMarker;
          DomainSendCount[iDomain]++;
        }
        if (Marker_All_SendRecv[iMarker] == -iDomain) {
          DomainReceiveMarkers[iDomain][DomainReceiveCount[iDomain]] = iMarker;
          DomainReceiveCount[iDomain]++;
        }
      }
    }
  }
  
  /*--- Create an structure to store the Send/Receive
   boundaries, because they require some reorganization ---*/
  
  nMarker_SendRecv = nMarker - nMarker_Physical - Duplicate_SendReceive;
  bound_Copy = new CPrimalGrid**[nMarker_Physical + nMarker_SendRecv];
  nElem_Bound_Copy = new unsigned long [nMarker_Physical + nMarker_SendRecv];
  Marker_All_SendRecv_Copy = new short [nMarker_Physical + nMarker_SendRecv];
  iMarker_ = nMarker_Physical;
  iVertex_ = 0;
  CheckStart = false;
  
  /*--- Copy and allocate the physical markers in the data structure ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (bound[iMarker][0]->GetVTK_Type() != VERTEX) {
      
      nElem_Bound_Copy[iMarker] = nElem_Bound[iMarker];
      bound_Copy[iMarker] = new CPrimalGrid* [nElem_Bound[iMarker]];
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        if (bound[iMarker][iElem_Bound]->GetVTK_Type() == LINE)
          bound_Copy[iMarker][iElem_Bound] = new CLine(bound[iMarker][iElem_Bound]->GetNode(0),
                                                       bound[iMarker][iElem_Bound]->GetNode(1), 2);
        if (bound[iMarker][iElem_Bound]->GetVTK_Type() == TRIANGLE)
          
          bound_Copy[iMarker][iElem_Bound] = new CTriangle(bound[iMarker][iElem_Bound]->GetNode(0),
                                                           bound[iMarker][iElem_Bound]->GetNode(1),
                                                           bound[iMarker][iElem_Bound]->GetNode(2), 3);
        if (bound[iMarker][iElem_Bound]->GetVTK_Type() == QUADRILATERAL)
          bound_Copy[iMarker][iElem_Bound] = new CQuadrilateral(bound[iMarker][iElem_Bound]->GetNode(0),
                                                            bound[iMarker][iElem_Bound]->GetNode(1),
                                                            bound[iMarker][iElem_Bound]->GetNode(2),
                                                            bound[iMarker][iElem_Bound]->GetNode(3), 3);
      }
    }
  }
  
  
  for (iDomain = 0; iDomain < nDomain; iDomain++) {
    
    /*--- Compute the total number of elements (adding all the
     boundaries with the same Send/Receive ---*/
    
    if (DomainSendCount[iDomain] != 0) {
      TotalElem = 0;
      for (iMarkersDomain = 0; iMarkersDomain < DomainSendCount[iDomain]; iMarkersDomain++) {
        iMarker = DomainSendMarkers[iDomain][iMarkersDomain];
        TotalElem += nElem_Bound[iMarker];
      }
      if (CheckStart) iMarker_++;
      CheckStart = true;
      iVertex_ = 0;
      nElem_Bound_Copy[iMarker_] = TotalElem;
      bound_Copy[iMarker_] = new CPrimalGrid*[TotalElem];
    }
    
    for (iMarkersDomain = 0; iMarkersDomain < DomainSendCount[iDomain]; iMarkersDomain++) {
      iMarker = DomainSendMarkers[iDomain][iMarkersDomain];
      Marker_All_SendRecv_Copy[iMarker_] = Marker_All_SendRecv[iMarker];
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        bound_Copy[iMarker_][iVertex_] = new CVertexMPI(bound[iMarker][iElem_Bound]->GetNode(0), nDim);
        bound_Copy[iMarker_][iVertex_]->SetRotation_Type(bound[iMarker][iElem_Bound]->GetRotation_Type());
        iVertex_++;
      }
      
    }
    
    /*--- Compute the total number of elements (adding all the
     boundaries with the same Send/Receive ---*/
    
    if (DomainReceiveCount[iDomain] != 0) {
      TotalElem = 0;
      for (iMarkersDomain = 0; iMarkersDomain < DomainReceiveCount[iDomain]; iMarkersDomain++) {
        iMarker = DomainReceiveMarkers[iDomain][iMarkersDomain];
        TotalElem += nElem_Bound[iMarker];
      }
      if (CheckStart) iMarker_++;
      CheckStart = true;
      iVertex_ = 0;
      nElem_Bound_Copy[iMarker_] = TotalElem;
      bound_Copy[iMarker_] = new CPrimalGrid*[TotalElem];
      
    }
    
    for (iMarkersDomain = 0; iMarkersDomain < DomainReceiveCount[iDomain]; iMarkersDomain++) {
      iMarker = DomainReceiveMarkers[iDomain][iMarkersDomain];
      Marker_All_SendRecv_Copy[iMarker_] = Marker_All_SendRecv[iMarker];
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        bound_Copy[iMarker_][iVertex_] = new CVertexMPI(bound[iMarker][iElem_Bound]->GetNode(0), nDim);
        bound_Copy[iMarker_][iVertex_]->SetRotation_Type(bound[iMarker][iElem_Bound]->GetRotation_Type());
        iVertex_++;
      }
      
    }
    
  }
  
  delete [] DomainSendCount;
  for (iDomain = 0; iDomain < nDomain; iDomain++)
    delete [] DomainSendMarkers[iDomain];
  delete[] DomainSendMarkers;
  
  delete [] DomainReceiveCount;
  for (iDomain = 0; iDomain < nDomain; iDomain++)
    delete [] DomainReceiveMarkers[iDomain];
  delete[] DomainReceiveMarkers;
  
   /*--- Deallocate the bound variables ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
   for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++)
     if (bound[iMarker][iElem_Bound] != NULL) delete bound[iMarker][iElem_Bound];
    if (bound[iMarker] != NULL) delete [] bound[iMarker];
  }
  if (bound != NULL) delete [] bound;
 
  /*--- Allocate the new bound variables, and set the number of markers ---*/
  
  bound = bound_Copy;
  nMarker = nMarker_Physical + nMarker_SendRecv;
  
  config->SetnMarker_All(nMarker);

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    nElem_Bound[iMarker] = nElem_Bound_Copy[iMarker];
  }
  for (iMarker = nMarker_Physical; iMarker < nMarker; iMarker++) {
    Marker_All_SendRecv[iMarker] = Marker_All_SendRecv_Copy[iMarker];
    config->SetMarker_All_SendRecv(iMarker, Marker_All_SendRecv[iMarker]);
    config->SetMarker_All_TagBound(iMarker, "SEND_RECEIVE");
  }
  
  /*--- Update config information storing the boundary information in the right place ---*/
  
  for (iMarker = 0 ; iMarker < nMarker; iMarker++) {
    
    string Marker_Tag = config->GetMarker_All_TagBound(iMarker);
    
    if (Marker_Tag != "SEND_RECEIVE") {
      
      /*--- Update config information storing the boundary information in the right place ---*/
      
      Tag_to_Marker[config->GetMarker_CfgFile_TagBound(Marker_Tag)] = Marker_Tag;
      config->SetMarker_All_KindBC(iMarker, config->GetMarker_CfgFile_KindBC(Marker_Tag));
      config->SetMarker_All_Monitoring(iMarker, config->GetMarker_CfgFile_Monitoring(Marker_Tag));
      config->SetMarker_All_GeoEval(iMarker, config->GetMarker_CfgFile_GeoEval(Marker_Tag));
      config->SetMarker_All_Designing(iMarker, config->GetMarker_CfgFile_Designing(Marker_Tag));
      config->SetMarker_All_Plotting(iMarker, config->GetMarker_CfgFile_Plotting(Marker_Tag));
      config->SetMarker_All_Analyze(iMarker, config->GetMarker_CfgFile_Analyze(Marker_Tag));
      config->SetMarker_All_ZoneInterface(iMarker, config->GetMarker_CfgFile_ZoneInterface(Marker_Tag));
      config->SetMarker_All_DV(iMarker, config->GetMarker_CfgFile_DV(Marker_Tag));
      config->SetMarker_All_Moving(iMarker, config->GetMarker_CfgFile_Moving(Marker_Tag));
      config->SetMarker_All_PerBound(iMarker, config->GetMarker_CfgFile_PerBound(Marker_Tag));
	  config->SetMarker_All_Turbomachinery(iMarker, config->GetMarker_CfgFile_Turbomachinery(Marker_Tag));
      config->SetMarker_All_TurbomachineryFlag(iMarker, config->GetMarker_CfgFile_TurbomachineryFlag(Marker_Tag));
      config->SetMarker_All_MixingPlaneInterface(iMarker, config->GetMarker_CfgFile_MixingPlaneInterface(Marker_Tag));
    }
    
    /*--- Send-Receive boundaries definition ---*/
    
    else {
      
      config->SetMarker_All_KindBC(iMarker, SEND_RECEIVE);
      config->SetMarker_All_Monitoring(iMarker, NO);
      config->SetMarker_All_GeoEval(iMarker, NO);
      config->SetMarker_All_Designing(iMarker, NO);
      config->SetMarker_All_Plotting(iMarker, NO);
      config->SetMarker_All_Analyze(iMarker, NO);
  	  config->SetMarker_All_ZoneInterface(iMarker, NO);
      config->SetMarker_All_DV(iMarker, NO);
      config->SetMarker_All_Moving(iMarker, NO);
      config->SetMarker_All_PerBound(iMarker, NO);
      config->SetMarker_All_Turbomachinery(iMarker, NO);
      config->SetMarker_All_TurbomachineryFlag(iMarker, NO);
      config->SetMarker_All_MixingPlaneInterface(iMarker, NO);
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        if (config->GetMarker_All_SendRecv(iMarker) < 0)
          node[bound[iMarker][iElem_Bound]->GetNode(0)]->SetDomain(false);
      }
      
    }
    
    /*--- Loop over the surface element to set the boundaries ---*/
    
    unsigned long Point_Surface, iElem_Surface;
    unsigned short iNode_Surface;
    
    for (iElem_Surface = 0; iElem_Surface < nElem_Bound[iMarker]; iElem_Surface++) {
      for (iNode_Surface = 0; iNode_Surface < bound[iMarker][iElem_Surface]->GetnNodes(); iNode_Surface++) {
        Point_Surface = bound[iMarker][iElem_Surface]->GetNode(iNode_Surface);
        node[Point_Surface]->SetBoundary(nMarker);
        if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE &&
            config->GetMarker_All_KindBC(iMarker) != INTERFACE_BOUNDARY &&
            config->GetMarker_All_KindBC(iMarker) != NEARFIELD_BOUNDARY &&
            config->GetMarker_All_KindBC(iMarker) != PERIODIC_BOUNDARY)
          node[Point_Surface]->SetPhysicalBoundary(true);
        
        if (config->GetMarker_All_KindBC(iMarker) == EULER_WALL &&
            config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX &&
            config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)
          node[Point_Surface]->SetSolidBoundary(true);
      }
    }
    
  }
  
  delete [] Marker_All_SendRecv_Copy;
  delete [] nElem_Bound_Copy;
}

void CPhysicalGeometry::Read_SU2_Format_Parallel(CConfig *config, string val_mesh_filename, unsigned short val_iZone, unsigned short val_nZone) {
  
  string text_line, Marker_Tag;
  ifstream mesh_file;
  unsigned short nMarker_Max = config->GetnMarker_Max();
  unsigned long VTK_Type, iMarker, iChar;
  unsigned long iCount = 0;
  unsigned long iElem_Bound = 0, iPoint = 0, ielem = 0;
  unsigned long vnodes_edge[2], vnodes_triangle[3], vnodes_quad[4];
  unsigned long vnodes_tetra[4], vnodes_hexa[8], vnodes_prism[6],
  vnodes_pyramid[5], dummyLong, GlobalIndex;
  unsigned long i;
  long local_index;
  vector<unsigned long>::iterator it;
  char cstr[200];
  su2double Coord_2D[2], Coord_3D[3], AoA_Offset, AoS_Offset, AoA_Current, AoS_Current;
  string::size_type position;
  int rank = MASTER_NODE, size = SINGLE_NODE;
  bool domain_flag = false;
  bool found_transform = false;
  bool harmonic_balance = config->GetUnsteady_Simulation() == HARMONIC_BALANCE;
  bool actuator_disk  = (((config->GetnMarker_ActDiskInlet() != 0) ||
                          (config->GetnMarker_ActDiskOutlet() != 0)) &&
                         ((config->GetKind_SU2() == SU2_CFD) ||
                          ((config->GetKind_SU2() == SU2_DEF) && (config->GetActDisk_SU2_DEF()))));
  if (config->GetActDisk_DoubleSurface()) actuator_disk = false;

  nZone = val_nZone;
  
  /*--- Initialize some additional counters for the parallel partitioning ---*/
  
  unsigned long total_pt_accounted = 0;
  unsigned long rem_points = 0;
  unsigned long element_count = 0;
  unsigned long boundary_marker_count = 0;
  unsigned long node_count = 0;
  unsigned long local_element_count = 0;
  
  /*--- Initialize counters for local/global points & elements ---*/
  
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  unsigned long LocalIndex, j;
#endif
  
  /*--- Actuator disk preprocesing ---*/
  
  string Marker_Tag_Duplicate;
  bool *ActDisk_Bool 			= NULL, *MapVolumePointBool = NULL, InElem, Perimeter;
  unsigned long *ActDiskPoint_Back = NULL, *VolumePoint_Inv = NULL, *ActDiskPoint_Front_Inv = NULL, ActDiskNewPoints = 0, Counter = 0;
  su2double *CoordXActDisk = NULL, *CoordYActDisk = NULL, *CoordZActDisk = NULL;
  su2double *CoordXVolumePoint = NULL, *CoordYVolumePoint = NULL, *CoordZVolumePoint = NULL;
  su2double Xloc = 0.0, Yloc = 0.0, Zloc = 0.0, Xcg = 0.0;
  unsigned long nElem_Bound_, kPoint;

  vector<unsigned long long> EdgeBegin, EdgeEnd;
  
  unsigned long AuxEdge, iEdge, jEdge, nEdges, nPointVolume, iElem;
  unsigned long long FirstEdgeIndex, SecondEdgeIndex;
  
  vector<unsigned long> ActDiskPoint_Front, VolumePoint, PerimeterPoint;
  
  /*--- If actuator disk, we should split the surface, the first step is to identify the existing boundary ---*/
  
  if (actuator_disk) {
    
    /*--- Open grid file ---*/
    
    strcpy (cstr, val_mesh_filename.c_str());
    mesh_file.open(cstr, ios::in);
    
    /*--- Check the grid ---*/
    
    if (mesh_file.fail()) {
      cout << "There is no mesh file (CPhysicalGeometry)!! " << cstr << endl;
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
    }
    
    /*--- Read grid file with format SU2 ---*/
    
    while (getline (mesh_file, text_line)) {
      
      position = text_line.find ("NDIME=",0);
      if (position != string::npos) {
        text_line.erase (0,6); nDim = atoi(text_line.c_str());
      }
      
      position = text_line.find ("NPOIN=",0);
      if (position != string::npos) {
        text_line.erase (0,6); stringstream test_line(text_line);
        iCount = 0; while (test_line >> dummyLong) iCount++;
        stringstream  stream_line(text_line);
        if (iCount == 2) {  stream_line >> nPoint;  stream_line >> nPointDomain; }
        else if (iCount == 1) { stream_line >> nPoint; }
        for (iPoint = 0; iPoint < nPoint; iPoint++) getline (mesh_file, text_line);
      }
      
      position = text_line.find ("NELEM=",0);
      if (position != string::npos) {
        text_line.erase (0,6); nElem = atoi(text_line.c_str());
        for (iElem = 0; iElem < nElem; iElem++) getline (mesh_file, text_line);
      }
      
      position = text_line.find ("NMARK=",0);
      if (position != string::npos) {
        text_line.erase (0,6); nMarker = atoi(text_line.c_str());
        
        for (iMarker = 0 ; iMarker < nMarker; iMarker++) {
          
          getline (mesh_file, text_line);
          text_line.erase (0,11); string::size_type position;
          for (iChar = 0; iChar < 20; iChar++) {
            position = text_line.find( " ", 0 );  if (position != string::npos) text_line.erase (position,1);
            position = text_line.find( "\r", 0 ); if (position != string::npos) text_line.erase (position,1);
            position = text_line.find( "\n", 0 ); if (position != string::npos) text_line.erase (position,1);
          }
          Marker_Tag = text_line.c_str();
          
          getline (mesh_file, text_line);
          text_line.erase (0,13); nElem_Bound_ = atoi(text_line.c_str());
          
          if (( Marker_Tag  == config->GetMarker_ActDiskInlet_TagBound(0)) && (rank == MASTER_NODE))
            cout << "Splitting the surface " << Marker_Tag << "( " << nElem_Bound_  << " boundary elements )." << endl;
          
          if (Marker_Tag  != config->GetMarker_ActDiskInlet_TagBound(0)) {
            for (iElem_Bound = 0; iElem_Bound < nElem_Bound_; iElem_Bound++) { getline (mesh_file, text_line); }
          }
          else {
            
            /*--- Create a list of edges ---*/
            
            for (iElem_Bound = 0; iElem_Bound < nElem_Bound_; iElem_Bound++) {
              
              getline(mesh_file, text_line);
              
              istringstream bound_line(text_line); bound_line >> VTK_Type;
              
              switch(VTK_Type) {
                case LINE:
                  bound_line >> vnodes_edge[0]; bound_line >> vnodes_edge[1];
                  EdgeBegin.push_back(vnodes_edge[0]); EdgeEnd.push_back(vnodes_edge[1]);
                  break;
                case TRIANGLE:
                  bound_line >> vnodes_triangle[0]; bound_line >> vnodes_triangle[1]; bound_line >> vnodes_triangle[2];
                  EdgeBegin.push_back(vnodes_triangle[0]); EdgeEnd.push_back(vnodes_triangle[1]);
                  EdgeBegin.push_back(vnodes_triangle[1]); EdgeEnd.push_back(vnodes_triangle[2]);
                  EdgeBegin.push_back(vnodes_triangle[2]); EdgeEnd.push_back(vnodes_triangle[0]);
                  break;
                case QUADRILATERAL:
                  bound_line >> vnodes_quad[0]; bound_line >> vnodes_quad[1]; bound_line >> vnodes_quad[2]; bound_line >> vnodes_quad[3];
                  EdgeBegin.push_back(vnodes_quad[0]); EdgeEnd.push_back(vnodes_quad[1]);
                  EdgeBegin.push_back(vnodes_quad[1]); EdgeEnd.push_back(vnodes_quad[2]);
                  EdgeBegin.push_back(vnodes_quad[2]); EdgeEnd.push_back(vnodes_quad[3]);
                  EdgeBegin.push_back(vnodes_quad[3]); EdgeEnd.push_back(vnodes_quad[0]);
                  break;
              }
              
            }
            
            /*--- Set the total number of edges ---*/
            
            nEdges = EdgeBegin.size();
            
            /*--- Sort edges based on local point index, first index is always the largest ---*/
            
            for (iEdge = 0; iEdge <  nEdges; iEdge++) {
              if (EdgeEnd[iEdge] < EdgeBegin[iEdge]) {
                AuxEdge = EdgeEnd[iEdge]; EdgeEnd[iEdge] = EdgeBegin[iEdge]; EdgeBegin[iEdge] = AuxEdge;
              }
            }
            
            /*--- Bubble sort of the points based on the first index   ---*/
            
            for (iEdge = 0; iEdge < nEdges; iEdge++) {
              for (jEdge = iEdge+1; jEdge < nEdges; jEdge++) {
                
                FirstEdgeIndex = EdgeBegin[jEdge] << 31;
                FirstEdgeIndex += EdgeEnd[jEdge];
                
                SecondEdgeIndex = EdgeBegin[iEdge] << 31;
                SecondEdgeIndex += EdgeEnd[iEdge];
                
                if (FirstEdgeIndex <= SecondEdgeIndex) {
                  AuxEdge = EdgeBegin[iEdge]; EdgeBegin[iEdge] = EdgeBegin[jEdge]; EdgeBegin[jEdge] = AuxEdge;
                  AuxEdge = EdgeEnd[iEdge];  EdgeEnd[iEdge] = EdgeEnd[jEdge]; EdgeEnd[jEdge] = AuxEdge;
                }
              }
            }
            
            if (nDim == 3) {
              
              /*--- Check the begning of the list ---*/
              
              if (!((EdgeBegin[0] == EdgeBegin[1]) && (EdgeEnd[0] == EdgeEnd[1]))) {
                PerimeterPoint.push_back(EdgeBegin[0]);
                PerimeterPoint.push_back(EdgeEnd[0]);
              }
              
              for (iEdge = 1; iEdge < nEdges-1; iEdge++) {
                bool Check_1 = !((EdgeBegin[iEdge] == EdgeBegin[iEdge-1]) && (EdgeEnd[iEdge] == EdgeEnd[iEdge-1]));
                bool Check_2 = !((EdgeBegin[iEdge] == EdgeBegin[iEdge+1]) && (EdgeEnd[iEdge] == EdgeEnd[iEdge+1]));
                if ((Check_1 && Check_2)) {
                  PerimeterPoint.push_back(EdgeBegin[iEdge]);
                  PerimeterPoint.push_back(EdgeEnd[iEdge]);
                }
              }
              
              /*--- Check the  end of the list ---*/
              
              if (!((EdgeBegin[nEdges-1] == EdgeBegin[nEdges-2]) && (EdgeEnd[nEdges-1] == EdgeEnd[nEdges-2]))) {
                PerimeterPoint.push_back(EdgeBegin[nEdges-1]);
                PerimeterPoint.push_back(EdgeEnd[nEdges-1]);
              }
              
            }
            
            else {
              
              
              /*--- Create a list with all the points ---*/
              
              for (iEdge = 0; iEdge < nEdges; iEdge++) {
                ActDiskPoint_Front.push_back(EdgeBegin[iEdge]);
                ActDiskPoint_Front.push_back(EdgeEnd[iEdge]);
              }
              
              sort(ActDiskPoint_Front.begin(), ActDiskPoint_Front.end());
              it = unique(ActDiskPoint_Front.begin(), ActDiskPoint_Front.end());
              ActDiskPoint_Front.resize(it - ActDiskPoint_Front.begin());
              
              /*--- Check the begning of the list ---*/
              
              if (!(ActDiskPoint_Front[0] == ActDiskPoint_Front[1]) ) { PerimeterPoint.push_back(ActDiskPoint_Front[0]); }
              
              for (iPoint = 1; iPoint < ActDiskPoint_Front.size()-1; iPoint++) {
                bool Check_1 = !((ActDiskPoint_Front[iPoint] == ActDiskPoint_Front[iPoint-1]) );
                bool Check_2 = !((ActDiskPoint_Front[iPoint] == ActDiskPoint_Front[iPoint+1]) );
                if ((Check_1 && Check_2)) { PerimeterPoint.push_back(ActDiskPoint_Front[iEdge]); }
              }
              
              /*--- Check the  end of the list ---*/
              
              if (!((EdgeBegin[ActDiskPoint_Front.size()-1] == EdgeBegin[ActDiskPoint_Front.size()-2]) )) {
                PerimeterPoint.push_back(ActDiskPoint_Front[ActDiskPoint_Front.size()-1]);
              }
              
              ActDiskPoint_Front.clear();
              
            }
            
            sort(PerimeterPoint.begin(), PerimeterPoint.end());
            it = unique(PerimeterPoint.begin(), PerimeterPoint.end());
            PerimeterPoint.resize(it - PerimeterPoint.begin());
            
            for (iEdge = 0; iEdge < nEdges; iEdge++) {
              
              Perimeter = false;
              for (iPoint = 0; iPoint < PerimeterPoint.size(); iPoint++) {
                if (EdgeBegin[iEdge] == PerimeterPoint[iPoint]) {
                  Perimeter = true; break;
                }
              }
              
              if (!Perimeter) ActDiskPoint_Front.push_back(EdgeBegin[iEdge]);
              
              Perimeter = false;
              for (iPoint = 0; iPoint < PerimeterPoint.size(); iPoint++) {
                if (EdgeEnd[iEdge] == PerimeterPoint[iPoint]) {
                  Perimeter = true; break;
                }
              }
              
              if (!Perimeter) ActDiskPoint_Front.push_back(EdgeEnd[iEdge]);
              
            }
            
            /*--- Sort, and remove repeated points from the disk list of points ---*/
            
            sort(ActDiskPoint_Front.begin(), ActDiskPoint_Front.end());
            it = unique(ActDiskPoint_Front.begin(), ActDiskPoint_Front.end());
            ActDiskPoint_Front.resize(it - ActDiskPoint_Front.begin());
            ActDiskNewPoints = ActDiskPoint_Front.size();
            
            if (rank == MASTER_NODE)
              cout << "Splitting the surface " << Marker_Tag << "( " << ActDiskPoint_Front.size()  << " internal points )." << endl;
            
            /*--- Create a map from original point to the new ones (back plane) ---*/
            
            ActDiskPoint_Back = new unsigned long [nPoint];
            ActDisk_Bool = new bool [nPoint];
            ActDiskPoint_Front_Inv= new unsigned long [nPoint];
            
            for (iPoint = 0; iPoint < nPoint; iPoint++) {
              ActDisk_Bool[iPoint] = false;
              ActDiskPoint_Back[iPoint] = 0;
            }
            
            kPoint = nPoint;
            for (iPoint = 0; iPoint < ActDiskPoint_Front.size(); iPoint++) {
              ActDiskPoint_Front_Inv[ActDiskPoint_Front[iPoint]] = iPoint;
              ActDisk_Bool[ActDiskPoint_Front[iPoint]] = true;
              ActDiskPoint_Back[ActDiskPoint_Front[iPoint]] = kPoint;
             	kPoint++;
            }
            
          }
        }
      }
    }
    
    mesh_file.close();
    
    /*--- Store the coordinates of the new points ---*/
    
    CoordXActDisk = new su2double[ActDiskNewPoints];
    CoordYActDisk = new su2double[ActDiskNewPoints];
    CoordZActDisk = new su2double[ActDiskNewPoints];
    
    strcpy (cstr, val_mesh_filename.c_str());
    mesh_file.open(cstr, ios::in);
    
    
    /*--- Read the coordinates of the points ---*/
    
    while (getline (mesh_file, text_line)) {
      
      position = text_line.find ("NPOIN=",0);
      if (position != string::npos) {
        text_line.erase (0,6); stringstream test_line(text_line);
        iCount = 0; while (test_line >> dummyLong) iCount++;
        stringstream  stream_line(text_line);
        if (iCount == 2) {  stream_line >> nPoint;  stream_line >> nPointDomain; }
        else if (iCount == 1) { stream_line >> nPoint; }
        
        Counter = 0;
        for (iPoint = 0; iPoint < nPoint; iPoint++) {
          getline (mesh_file, text_line);
          istringstream point_line(text_line);
          if (nDim == 2) {point_line >> Coord_2D[0]; point_line >> Coord_2D[1]; }
          else { point_line >> Coord_3D[0]; point_line >> Coord_3D[1]; point_line >> Coord_3D[2]; }
          
          /*--- Compute the CG of the actuator disk surface ---*/
          
          if (ActDisk_Bool[iPoint]) {
            CoordXActDisk[ActDiskPoint_Front_Inv[iPoint]] = Coord_3D[0];
            CoordYActDisk[ActDiskPoint_Front_Inv[iPoint]] = Coord_3D[1];
            Xloc += Coord_3D[0]; Yloc += Coord_3D[1];
            if (nDim == 3) {
              CoordZActDisk[ActDiskPoint_Front_Inv[iPoint]] = Coord_3D[2];
              Zloc += Coord_3D[2];
            }
            Counter++;
          }
          
        }
      }
      
      /*--- Find points that touch the actuator disk surface ---*/
      
      position = text_line.find ("NELEM=",0);
      if (position != string::npos) {
        text_line.erase (0,6); nElem = atoi(text_line.c_str());
        for (iElem = 0; iElem < nElem; iElem++) {
          
          getline(mesh_file, text_line);
          istringstream elem_line(text_line);
          
          elem_line >> VTK_Type;
          
          switch(VTK_Type) {
            case TRIANGLE:
              elem_line >> vnodes_triangle[0]; elem_line >> vnodes_triangle[1]; elem_line >> vnodes_triangle[2];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_TRIANGLE; i++) {
                if (ActDisk_Bool[vnodes_triangle[i]]) { InElem = true; break; } }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_TRIANGLE; i++) {
                  VolumePoint.push_back(vnodes_triangle[i]); } }
              break;
            case QUADRILATERAL:
              elem_line >> vnodes_quad[0]; elem_line >> vnodes_quad[1]; elem_line >> vnodes_quad[2]; elem_line >> vnodes_quad[3];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_QUADRILATERAL; i++) {
                if (ActDisk_Bool[vnodes_quad[i]]) { InElem = true; break; } }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_QUADRILATERAL; i++) {
                  VolumePoint.push_back(vnodes_quad[i]); } }
              break;
            case TETRAHEDRON:
              elem_line >> vnodes_tetra[0]; elem_line >> vnodes_tetra[1]; elem_line >> vnodes_tetra[2]; elem_line >> vnodes_tetra[3];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_TETRAHEDRON; i++) {
                if (ActDisk_Bool[vnodes_tetra[i]]) { InElem = true; break; }              }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_TETRAHEDRON; i++) {
                  VolumePoint.push_back(vnodes_tetra[i]); } }
              break;
            case HEXAHEDRON:
              elem_line >> vnodes_hexa[0]; elem_line >> vnodes_hexa[1]; elem_line >> vnodes_hexa[2];
              elem_line >> vnodes_hexa[3]; elem_line >> vnodes_hexa[4]; elem_line >> vnodes_hexa[5];
              elem_line >> vnodes_hexa[6]; elem_line >> vnodes_hexa[7];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_HEXAHEDRON; i++) {
                if (ActDisk_Bool[vnodes_hexa[i]]) { InElem = true; break; } }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_HEXAHEDRON; i++) {
                  VolumePoint.push_back(vnodes_hexa[i]); } }
              break;
            case PRISM:
              elem_line >> vnodes_prism[0]; elem_line >> vnodes_prism[1]; elem_line >> vnodes_prism[2];
              elem_line >> vnodes_prism[3]; elem_line >> vnodes_prism[4]; elem_line >> vnodes_prism[5];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_PRISM; i++) {
                if (ActDisk_Bool[vnodes_prism[i]]) { InElem = true; break; } }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_PRISM; i++) {
                  VolumePoint.push_back(vnodes_prism[i]); } }
              break;
            case PYRAMID:
              elem_line >> vnodes_pyramid[0]; elem_line >> vnodes_pyramid[1]; elem_line >> vnodes_pyramid[2];
              elem_line >> vnodes_pyramid[3]; elem_line >> vnodes_pyramid[4];
              InElem = false;
              for (i = 0; i < (unsigned long)N_POINTS_PYRAMID; i++) {
                if (ActDisk_Bool[vnodes_pyramid[i]]) { InElem = true; break; } }
              if (InElem) {
                for (i = 0; i < (unsigned long)N_POINTS_PYRAMID; i++) {
                  VolumePoint.push_back(vnodes_pyramid[i]); } }
              break;
          }
        }
      }
      
    }
    
    mesh_file.close();
    
    /*--- Compute the CG of the surface ---*/
    
    Xloc /= su2double(Counter);  Yloc /= su2double(Counter);  Zloc /= su2double(Counter);
    
    /*--- Sort,and remove repeated points from the disk list of points ---*/
    
    sort(VolumePoint.begin(), VolumePoint.end());
    it = unique(VolumePoint.begin(), VolumePoint.end());
    VolumePoint.resize(it - VolumePoint.begin());
    nPointVolume = VolumePoint.size();
    
    
    CoordXVolumePoint = new su2double[nPointVolume];
    CoordYVolumePoint = new su2double[nPointVolume];
    CoordZVolumePoint = new su2double[nPointVolume];
    MapVolumePointBool = new bool[nPoint];
    VolumePoint_Inv = new unsigned long[nPoint];
    
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      MapVolumePointBool[iPoint] = false;
    }
    
    for (iPoint = 0; iPoint < nPointVolume; iPoint++) {
      MapVolumePointBool[VolumePoint[iPoint]] = true;
      VolumePoint_Inv[VolumePoint[iPoint]] = iPoint;
    }
    
    strcpy (cstr, val_mesh_filename.c_str());
    mesh_file.open(cstr, ios::in);
    
    /*--- Store the coordinates of all the surface and volume
     points that touch the actuator disk ---*/
    
    while (getline (mesh_file, text_line)) {
      
      position = text_line.find ("NPOIN=",0);
      if (position != string::npos) {
        text_line.erase (0,6); stringstream test_line(text_line);
        iCount = 0; while (test_line >> dummyLong) iCount++;
        stringstream  stream_line(text_line);
        if (iCount == 2) {  stream_line >> nPoint;  stream_line >> nPointDomain; }
        else if (iCount == 1) { stream_line >> nPoint; }
        
        Counter =0;
        for (iPoint = 0; iPoint < nPoint; iPoint++) {
          getline (mesh_file, text_line);
          istringstream point_line(text_line);
          if (nDim == 2) {point_line >> Coord_2D[0]; point_line >> Coord_2D[1]; }
          else { point_line >> Coord_3D[0]; point_line >> Coord_3D[1]; point_line >> Coord_3D[2]; }
          
          if (MapVolumePointBool[iPoint]) {
            CoordXVolumePoint[VolumePoint_Inv[iPoint]] = Coord_3D[0];
            CoordYVolumePoint[VolumePoint_Inv[iPoint]] = Coord_3D[1];
            if (nDim == 3) { CoordZVolumePoint[VolumePoint_Inv[iPoint]] = Coord_3D[2]; }
          }
        }
      }
      
    }
    
    mesh_file.close();
    
    /*--- Deallocate memory ---*/
    
    delete [] MapVolumePointBool;
    delete [] ActDiskPoint_Front_Inv;
    
    //    }
    
    //  	/*--- Allocate and Send-Receive some of the vectors that we have computed on the MASTER_NODE ---*/
    
    //    SU2_MPI::Bcast(&ActDiskNewPoints, 1, MPI_UNSIGNED_LONG, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(&nPoint, 1, MPI_UNSIGNED_LONG, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(&nPointVolume, 1, MPI_UNSIGNED_LONG, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(&Xloc, 1, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(&Yloc, 1, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(&Zloc, 1, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    
    //    if (rank != MASTER_NODE) {
    //      MapActDisk 				= new unsigned long [nPoint];
    //      ActDisk_Bool 		= new bool [nPoint];
    //      VolumePoint_Inv 	= new unsigned long [nPoint];
    //      CoordXVolumePoint = new su2double [nPointVolume];
    //      CoordYVolumePoint = new su2double [nPointVolume];
    //      CoordZVolumePoint = new su2double [nPointVolume];
    //      CoordXActDisk 		= new su2double[ActDiskNewPoints];
    //      CoordYActDisk 		= new su2double[ActDiskNewPoints];
    //      CoordZActDisk 		= new su2double[ActDiskNewPoints];
    //    }
    
    //    SU2_MPI::Bcast(MapActDisk, nPoint, MPI_UNSIGNED_LONG, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(ActDisk_Bool, nPoint, MPI_UNSIGNED_SHORT, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(VolumePoint_Inv, nPoint, MPI_UNSIGNED_LONG, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordXVolumePoint, nPointVolume, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordYVolumePoint, nPointVolume, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordZVolumePoint, nPointVolume, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordXActDisk, ActDiskNewPoints, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordYActDisk, ActDiskNewPoints, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    //    SU2_MPI::Bcast(CoordZActDisk, ActDiskNewPoints, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    
  }
  
  Global_nPoint  = 0; Global_nPointDomain   = 0; Global_nElem = 0; Global_nElemDomain = 0;
  nelem_edge     = 0; Global_nelem_edge     = 0;
  nelem_triangle = 0; Global_nelem_triangle = 0;
  nelem_quad     = 0; Global_nelem_quad     = 0;
  nelem_tetra    = 0; Global_nelem_tetra    = 0;
  nelem_hexa     = 0; Global_nelem_hexa     = 0;
  nelem_prism    = 0; Global_nelem_prism    = 0;
  nelem_pyramid  = 0; Global_nelem_pyramid  = 0;
  
  /*--- Allocate memory for the linear partition of the mesh. These
   arrays are the size of the number of ranks. ---*/
  
  starting_node = new unsigned long[size];
  ending_node   = new unsigned long[size];
  npoint_procs  = new unsigned long[size];
  
  /*--- Open grid file ---*/
  
  strcpy (cstr, val_mesh_filename.c_str());
  mesh_file.open(cstr, ios::in);
  
  /*--- Check the grid ---*/
  
  if (mesh_file.fail()) {
    cout << "There is no mesh file (CPhysicalGeometry)!! " << cstr << endl;
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }
  
  /*--- If more than one, find the zone in the mesh file ---*/
  
  if (val_nZone > 1 || harmonic_balance) {
    if (harmonic_balance) {
      if (rank == MASTER_NODE) cout << "Reading time instance " << val_iZone+1 << ":" << endl;
    } else {
      while (getline (mesh_file,text_line)) {
        /*--- Search for the current domain ---*/
        position = text_line.find ("IZONE=",0);
        if (position != string::npos) {
          text_line.erase (0,6);
          unsigned short jDomain = atoi(text_line.c_str());
          if (jDomain == val_iZone+1) {
            if (rank == MASTER_NODE) cout << "Reading zone " << val_iZone+1 << " points:" << endl;
            break;
          }
        }
      }
    }
  }
  
  /*--- Read grid file with format SU2 ---*/
  
  while (getline (mesh_file, text_line)) {
    
    /*--- Read the dimension of the problem ---*/
    
    position = text_line.find ("NDIME=",0);
    if (position != string::npos) {
      if (domain_flag == false) {
        text_line.erase (0,6); nDim = atoi(text_line.c_str());
        if (rank == MASTER_NODE) {
          if (nDim == 2) cout << "Two dimensional problem." << endl;
          if (nDim == 3) cout << "Three dimensional problem." << endl;
        }
        domain_flag = true;
      } else { break; }
    }
    
    /*--- Read if there is any offset in the mesh parameters ---*/
    
    position = text_line.find ("AOA_OFFSET=",0);
    if (position != string::npos) {
      AoA_Offset = 0.0;
      text_line.erase (0,11); AoA_Offset = atof(text_line.c_str());
      
      /*--- The offset is in deg ---*/
      
      AoA_Current = config->GetAoA() + AoA_Offset;
      
      if (config->GetDiscard_InFiles() == false) {
        if ((rank == MASTER_NODE) && (AoA_Offset != 0.0))  {
          cout.precision(6);
          cout << fixed <<"WARNING: AoA in the config file (" << config->GetAoA() << " deg.) +" << endl;
          cout << "         AoA offset in mesh file (" << AoA_Offset << " deg.) = " << AoA_Current << " deg." << endl;
        }
        config->SetAoA_Offset(AoA_Offset);
        config->SetAoA(AoA_Current);
      }
      else {
        if ((rank == MASTER_NODE) && (AoA_Offset != 0.0))
          cout <<"WARNING: Discarding the AoA offset in the geometry file." << endl;
      }
      
    }
    
    position = text_line.find ("AOS_OFFSET=",0);
    if (position != string::npos) {
      AoS_Offset = 0.0;
      text_line.erase (0,11); AoS_Offset = atof(text_line.c_str());
      
      /*--- The offset is in deg ---*/
      
      AoS_Current = config->GetAoS() + AoS_Offset;
      
      if (config->GetDiscard_InFiles() == false) {
        if ((rank == MASTER_NODE) && (AoS_Offset != 0.0))  {
          cout.precision(6);
          cout << fixed <<"WARNING: AoS in the config file (" << config->GetAoS() << " deg.) +" << endl;
          cout << "         AoS offset in mesh file (" << AoS_Offset << " deg.) = " << AoS_Current << " deg." << endl;
        }
        config->SetAoS_Offset(AoS_Offset);
        config->SetAoS(AoS_Current);
      }
      else {
        if ((rank == MASTER_NODE) && (AoS_Offset != 0.0))
          cout <<"WARNING: Discarding the AoS offset in the geometry file." << endl;
      }
      
    }
    
    /*--- Read number of points ---*/
    
    position = text_line.find ("NPOIN=",0);
    if (position != string::npos) {
      text_line.erase (0,6);
      
      /*--- Check for ghost points. ---*/
      stringstream test_line(text_line);
      iCount = 0; while (test_line >> dummyLong) iCount++;
      
      /*--- Now read and store the number of points and possible ghost points. ---*/
      
      stringstream  stream_line(text_line);
      if (iCount == 2) {
        
        stream_line >> nPoint;
        stream_line >> nPointDomain;
        
        if (actuator_disk) { nPoint += ActDiskNewPoints;  nPointDomain += ActDiskNewPoints; }
        
        /*--- Set some important point information for parallel simulations. ---*/
        
        Global_nPoint = nPoint;
        Global_nPointDomain = nPointDomain;
        if (rank == MASTER_NODE && size > SINGLE_NODE) {
          cout << Global_nPointDomain << " points and " << Global_nPoint-Global_nPointDomain;
          cout << " ghost points before parallel partitioning." << endl;
        } else if (rank == MASTER_NODE) {
          cout << Global_nPointDomain << " points and " << Global_nPoint-Global_nPointDomain;
          cout << " ghost points." << endl;
        }
        
      } else if (iCount == 1) {
        stream_line >> nPoint;
        
        if (actuator_disk) { nPoint += ActDiskNewPoints; }
        
        nPointDomain = nPoint;
        Global_nPointDomain = nPoint;
        Global_nPoint = nPoint;
        if (rank == MASTER_NODE && size > SINGLE_NODE) {
          cout << nPoint << " points before parallel partitioning." << endl;
        } else if (rank == MASTER_NODE) {
          cout << nPoint << " points." << endl;
        }
      }
      else {
        cout << "NPOIN improperly specified!!" << endl;
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }
      
      if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
        cout << "Performing linear partitioning of the grid nodes." << endl;
      
      /*--- Compute the number of points that will be on each processor.
       This is a linear partitioning with the addition of a simple load
       balancing for any remainder points. ---*/
      
      total_pt_accounted = 0;
      for (i = 0; i < (unsigned long)size; i++) {
        npoint_procs[i] = nPoint/size;
        total_pt_accounted = total_pt_accounted + npoint_procs[i];
      }
      
      /*--- Get the number of remainder points after the even division ---*/
      
      rem_points = nPoint-total_pt_accounted;
      for (i = 0; i<rem_points; i++) {
        npoint_procs[i]++;
      }
      
      /*--- Store the local number of nodes and the beginning/end index.
       nPoint is always used to store the local number of points. ---*/
      
      nPoint = npoint_procs[rank];
      starting_node[0] = 0;
      ending_node[0]   = starting_node[0] + npoint_procs[0];
      for (unsigned long i = 1; i < (unsigned long)size; i++) {
        starting_node[i] = ending_node[i-1];
        ending_node[i]   = starting_node[i] + npoint_procs[i] ;
      }
      
      /*--- Here we check if a point in the mesh file lies in the domain
       and if so, then store it on the local processor. We only create enough
       space in the node container for the local nodes at this point. ---*/
      
      nPointNode = nPoint; 
      node = new CPoint*[nPoint];
      iPoint = 0; node_count = 0;
      while (node_count < Global_nPoint) {
        
        if (!actuator_disk) { getline(mesh_file, text_line); }
        else {
          if (node_count < Global_nPoint-ActDiskNewPoints) {
            getline(mesh_file, text_line);
          }
          else {
            ostringstream strsX, strsY, strsZ;
            unsigned long BackActDisk_Index = node_count;
            unsigned long LocalIndex = BackActDisk_Index - (Global_nPoint-ActDiskNewPoints);
            strsX.precision(20); strsY.precision(20); strsZ.precision(20);
            su2double CoordX = CoordXActDisk[LocalIndex]; strsX << scientific << CoordX;
            su2double CoordY = CoordYActDisk[LocalIndex]; strsY << scientific << CoordY;
            su2double CoordZ = CoordZActDisk[LocalIndex]; strsZ << scientific << CoordZ;
            text_line = strsX.str() + "\t" + strsY.str() + "\t" + strsZ.str();
          }
        }

        istringstream point_line(text_line);
        
        /*--- We only read information for this node if it is owned by this
         rank based upon our initial linear partitioning. ---*/
        
        if ((node_count >= starting_node[rank]) && (node_count < ending_node[rank])) {
          switch(nDim) {
            case 2:
              GlobalIndex = node_count;
#ifndef HAVE_MPI
              point_line >> Coord_2D[0]; point_line >> Coord_2D[1];
#else
              if (size > SINGLE_NODE) { point_line >> Coord_2D[0]; point_line >> Coord_2D[1]; point_line >> LocalIndex; point_line >> GlobalIndex; }
              else { point_line >> Coord_2D[0]; point_line >> Coord_2D[1]; LocalIndex = iPoint; GlobalIndex = node_count; }
#endif
              node[iPoint] = new CPoint(Coord_2D[0], Coord_2D[1], GlobalIndex, config);
              iPoint++; break;
            case 3:
              GlobalIndex = node_count;
#ifndef HAVE_MPI
              point_line >> Coord_3D[0]; point_line >> Coord_3D[1]; point_line >> Coord_3D[2];
#else
              if (size > SINGLE_NODE) { point_line >> Coord_3D[0]; point_line >> Coord_3D[1]; point_line >> Coord_3D[2]; point_line >> LocalIndex; point_line >> GlobalIndex; }
              else { point_line >> Coord_3D[0]; point_line >> Coord_3D[1]; point_line >> Coord_3D[2]; LocalIndex = iPoint; GlobalIndex = node_count; }
#endif
              node[iPoint] = new CPoint(Coord_3D[0], Coord_3D[1], Coord_3D[2], GlobalIndex, config);
              iPoint++; break;
          }
        }
        node_count++;
      }
    }
  }
  
  mesh_file.close();
  strcpy (cstr, val_mesh_filename.c_str());
  
  /*--- Read the elements in the file with two loops. The first finds
   elements that live in the local partition and builds the adjacency
   for ParMETIS (if parallel). Once we know how many elements we have
   on the local partition, we allocate memory and store those elements. ---*/
  
  map<unsigned long,bool> ElemIn;
  map<unsigned long, bool>::const_iterator MI;
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
  /*--- Initialize a vector for the adjacency information (ParMETIS). ---*/
  vector< vector<unsigned long> > adj_nodes(nPoint, vector<unsigned long>(0));
#endif
#endif
  
  /*--- Open the mesh file and find the section with the elements. ---*/
  
  mesh_file.open(cstr, ios::in);
  
  /*--- If more than one, find the zone in the mesh file  ---*/

  if (val_nZone > 1 && !harmonic_balance) {
    while (getline (mesh_file,text_line)) {
      /*--- Search for the current domain ---*/
      position = text_line.find ("IZONE=",0);
      if (position != string::npos) {
        text_line.erase (0,6);
        unsigned short jDomain = atoi(text_line.c_str());
        if (jDomain == val_iZone+1) {
          if (rank == MASTER_NODE) cout << "Reading zone " << val_iZone+1 << " elements:" << endl;
          break;
        }
      }
    }
  }
  
  while (getline (mesh_file, text_line)) {
    
    /*--- Read the information about inner elements ---*/
    
    position = text_line.find ("NELEM=",0);
    if (position != string::npos) {
      text_line.erase (0,6); nElem = atoi(text_line.c_str());
      
      /*--- Store total number of elements in the original mesh ---*/
      
      Global_nElem = nElem;
      if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
        cout << Global_nElem << " interior elements before parallel partitioning." << endl;
      
      /*--- Loop over all the volumetric elements and store any element that
       contains at least one of an owned node for this rank (i.e., there will
       be element redundancy, since multiple ranks will store the same elems
       on the boundaries of the initial linear partitioning. ---*/
      
      element_count = 0; local_element_count = 0;
      while (element_count < Global_nElem) {
        getline(mesh_file, text_line);
        istringstream elem_line(text_line);
        
        /*--- Decide whether this rank needs each element. If so, build the
         adjacency arrays needed by ParMETIS and store the element connectivity.
         Note that every proc starts it's node indexing from zero. ---*/
        
        elem_line >> VTK_Type;
        switch(VTK_Type) {
            
          case TRIANGLE:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_triangle[0];
            elem_line >> vnodes_triangle[1];
            elem_line >> vnodes_triangle[2];
            
            if (actuator_disk) {
              for (unsigned short i = 0; i<N_POINTS_TRIANGLE; i++) {
                if (ActDisk_Bool[vnodes_triangle[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_TRIANGLE; j++) {
                    if (vnodes_triangle[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_triangle[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) {
                      vnodes_triangle[i] = ActDiskPoint_Back[vnodes_triangle[i]];
                    }
                    else { vnodes_triangle[i] = vnodes_triangle[i]; }
                  }
                  
                }
              }
            }
            
            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_TRIANGLE; i++) {
              
              local_index = vnodes_triangle[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                for (unsigned short j=0; j<N_POINTS_TRIANGLE; j++) {
                  if (i != j) adj_nodes[local_index].push_back(vnodes_triangle[j]);
                }
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
            
          case QUADRILATERAL:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_quad[0];
            elem_line >> vnodes_quad[1];
            elem_line >> vnodes_quad[2];
            elem_line >> vnodes_quad[3];
            
            if (actuator_disk) {
              for (unsigned short  i = 0; i<N_POINTS_QUADRILATERAL; i++) {
                if (ActDisk_Bool[vnodes_quad[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_QUADRILATERAL; j++) {
                    if (vnodes_quad[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_quad[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) {
                      vnodes_quad[i] = ActDiskPoint_Back[vnodes_quad[i]];
                    }
                    else { vnodes_quad[i] = vnodes_quad[i]; }
                  }
                  
                }
              }
            }

            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_QUADRILATERAL; i++) {
              
              local_index = vnodes_quad[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                adj_nodes[local_index].push_back(vnodes_quad[(i+1)%4]);
                adj_nodes[local_index].push_back(vnodes_quad[(i+3)%4]);
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
            
          case TETRAHEDRON:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_tetra[0];
            elem_line >> vnodes_tetra[1];
            elem_line >> vnodes_tetra[2];
            elem_line >> vnodes_tetra[3];
            
            if (actuator_disk) {
              for (unsigned short  i = 0; i<N_POINTS_TETRAHEDRON; i++) {
                if (ActDisk_Bool[vnodes_tetra[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_TETRAHEDRON; j++) {
                    if (vnodes_tetra[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_tetra[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) {
                      vnodes_tetra[i] = ActDiskPoint_Back[vnodes_tetra[i]];
                    }
                    else { vnodes_tetra[i] = vnodes_tetra[i]; }
                  }
                  
                }
              }
            }
            
            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_TETRAHEDRON; i++) {
              
              local_index = vnodes_tetra[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                for (unsigned short j=0; j<N_POINTS_TETRAHEDRON; j++) {
                  if (i != j) adj_nodes[local_index].push_back(vnodes_tetra[j]);
                }
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
            
          case HEXAHEDRON:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_hexa[0];
            elem_line >> vnodes_hexa[1];
            elem_line >> vnodes_hexa[2];
            elem_line >> vnodes_hexa[3];
            elem_line >> vnodes_hexa[4];
            elem_line >> vnodes_hexa[5];
            elem_line >> vnodes_hexa[6];
            elem_line >> vnodes_hexa[7];
            
            if (actuator_disk) {
              for (unsigned short  i = 0; i<N_POINTS_HEXAHEDRON; i++) {
                if (ActDisk_Bool[vnodes_hexa[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_HEXAHEDRON; j++) {
                    if (vnodes_hexa[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_hexa[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) { vnodes_hexa[i] = ActDiskPoint_Back[vnodes_hexa[i]]; }
                    else { vnodes_hexa[i] = vnodes_hexa[i]; }
                  }
                }
              }
            }

            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_HEXAHEDRON; i++) {
              
              local_index = vnodes_hexa[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                if (i < 4) {
                  adj_nodes[local_index].push_back(vnodes_hexa[(i+1)%4]);
                  adj_nodes[local_index].push_back(vnodes_hexa[(i+3)%4]);
                } else {
                  adj_nodes[local_index].push_back(vnodes_hexa[(i-3)%4+4]);
                  adj_nodes[local_index].push_back(vnodes_hexa[(i-1)%4+4]);
                }
                adj_nodes[local_index].push_back(vnodes_hexa[(i+4)%8]);
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
            
          case PRISM:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_prism[0];
            elem_line >> vnodes_prism[1];
            elem_line >> vnodes_prism[2];
            elem_line >> vnodes_prism[3];
            elem_line >> vnodes_prism[4];
            elem_line >> vnodes_prism[5];
            
            if (actuator_disk) {
              for (unsigned short i = 0; i<N_POINTS_PRISM; i++) {
                if (ActDisk_Bool[vnodes_prism[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_PRISM; j++) {
                    if (vnodes_prism[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_prism[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) { vnodes_prism[i] = ActDiskPoint_Back[vnodes_prism[i]]; }
                    else { vnodes_prism[i] = vnodes_prism[i]; }
                  }
                }
              }
            }

            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_PRISM; i++) {
              
              local_index = vnodes_prism[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                if (i < 3) {
                  adj_nodes[local_index].push_back(vnodes_prism[(i+1)%3]);
                  adj_nodes[local_index].push_back(vnodes_prism[(i+2)%3]);
                } else {
                  adj_nodes[local_index].push_back(vnodes_prism[(i-2)%3+3]);
                  adj_nodes[local_index].push_back(vnodes_prism[(i-1)%3+3]);
                }
                adj_nodes[local_index].push_back(vnodes_prism[(i+3)%6]);
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
            
          case PYRAMID:
            
            /*--- Load the connectivity for this element. ---*/
            
            elem_line >> vnodes_pyramid[0];
            elem_line >> vnodes_pyramid[1];
            elem_line >> vnodes_pyramid[2];
            elem_line >> vnodes_pyramid[3];
            elem_line >> vnodes_pyramid[4];
            
            if (actuator_disk) {
              for (unsigned short i = 0; i<N_POINTS_PYRAMID; i++) {
                if (ActDisk_Bool[vnodes_pyramid[i]]) {
                  
                  Xcg = 0.0; Counter = 0;
                  for (unsigned short j = 0; j<N_POINTS_PYRAMID; j++) {
                    if (vnodes_pyramid[j] < Global_nPoint-ActDiskNewPoints) {
                      Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_pyramid[j]]];
                      Counter++;
                    }
                  }
                  Xcg = Xcg / su2double(Counter);
                  
                  if (Counter != 0)  {
                    if (Xcg > Xloc) { vnodes_pyramid[i] = ActDiskPoint_Back[vnodes_pyramid[i]]; }
                    else { vnodes_pyramid[i] = vnodes_pyramid[i]; }
                  }
                }
              }
            }

            /*--- Decide whether we need to store this element, i.e., check if
             any of the nodes making up this element have a global index value
             that falls within the range of our linear partitioning. ---*/
            
            for (unsigned short i = 0; i < N_POINTS_PYRAMID; i++) {
              
              local_index = vnodes_pyramid[i]-starting_node[rank];
              
              if ((local_index >= 0) && (local_index < (long)nPoint)) {
                
                /*--- This node is within our linear partition. Mark this
                 entire element to be added to our list for this rank, and
                 add the neighboring nodes to this nodes' adjacency list. ---*/
                
                ElemIn[element_count] = true;
                
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
                /*--- Build adjacency assuming the VTK connectivity ---*/
                if (i < 4) {
                  adj_nodes[local_index].push_back(vnodes_pyramid[(i+1)%4]);
                  adj_nodes[local_index].push_back(vnodes_pyramid[(i+3)%4]);
                  adj_nodes[local_index].push_back(vnodes_pyramid[4]);
                } else {
                  adj_nodes[local_index].push_back(vnodes_pyramid[0]);
                  adj_nodes[local_index].push_back(vnodes_pyramid[1]);
                  adj_nodes[local_index].push_back(vnodes_pyramid[2]);
                  adj_nodes[local_index].push_back(vnodes_pyramid[3]);
                }
#endif
#endif
              }
            }
            
            MI = ElemIn.find(element_count);
            if (MI != ElemIn.end()) local_element_count++;
            
            break;
        }
        element_count++;
      }
      if (element_count == Global_nElem) break;
    }
  }
  
  mesh_file.close();
  
  /*--- Store the number of elements on the whole domain, excluding halos. ---*/

  Global_nElemDomain = element_count;

  /*--- Store the number of local elements on each rank after determining
   which elements must be kept in the loop above. ---*/
  
  nElem = local_element_count;
  
  /*--- Begin dealing with the partitioning by adjusting the adjacency
   information and clear out memory where possible. ---*/
  
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
    cout << "Calling the partitioning functions." << endl;
  
  /*--- Post process the adjacency information in order to get it into the
   proper format before sending the data to ParMETIS. We need to remove
   repeats and adjust the size of the array for each local node. ---*/
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
    cout << "Building the graph adjacency structure." << endl;
  
  unsigned long loc_adjc_size=0;
  vector<unsigned long> adjac_vec;
  unsigned long adj_elem_size;
  
  xadj = new idx_t [npoint_procs[rank]+1];
  xadj[0]=0;
  vector<unsigned long> temp_adjacency;
  unsigned long local_count=0;
  
  /*--- Here, we transfer the adjacency information from a multi-dim vector
   on a node-by-node basis into a single vector container. First, we sort
   the entries and remove the duplicates we find for each node, then we
   copy it into the single vect and clear memory from the multi-dim vec. ---*/
  
  for (unsigned long i = 0; i < nPoint; i++) {
    
    for (j = 0; j<adj_nodes[i].size(); j++) {
      temp_adjacency.push_back(adj_nodes[i][j]);
    }
    
    sort(temp_adjacency.begin(), temp_adjacency.end());
    it = unique(temp_adjacency.begin(), temp_adjacency.end());
    loc_adjc_size = it - temp_adjacency.begin();
    
    temp_adjacency.resize(loc_adjc_size);
    xadj[local_count+1]=xadj[local_count]+loc_adjc_size;
    local_count++;
    
    for (j = 0; j<loc_adjc_size; j++) {
      adjac_vec.push_back(temp_adjacency[j]);
    }
    
    temp_adjacency.clear();
    adj_nodes[i].clear();
    
  }
  
  /*--- Now that we know the size, create the final adjacency array. This
   is the array that we will feed to ParMETIS for partitioning. ---*/
  
  adj_elem_size = xadj[npoint_procs[rank]];
  adjacency = new idx_t [adj_elem_size];
  copy(adjac_vec.begin(), adjac_vec.end(), adjacency);
  
  xadj_size = npoint_procs[rank]+1;
  adjacency_size = adj_elem_size;
  
  /*--- Free temporary memory used to build the adjacency. ---*/
  
  adjac_vec.clear();
  adj_nodes.clear();
  
#endif
#endif
  
  /*--- Open the mesh file again and now that we know the number of
   elements needed on each partition, allocate memory for them. ---*/
  
  mesh_file.open(cstr, ios::in);
  
  /*--- If more than one, find the zone in the mesh file  ---*/
  
  if (val_nZone > 1 && !harmonic_balance) {
    while (getline (mesh_file,text_line)) {
      /*--- Search for the current domain ---*/
      position = text_line.find ("IZONE=",0);
      if (position != string::npos) {
        text_line.erase (0,6);
        unsigned short jDomain = atoi(text_line.c_str());
        if (jDomain == val_iZone+1) {
          if (rank == MASTER_NODE) cout << "Reading zone " << val_iZone+1 << " elements:" << endl;
          break;
        }
      }
    }
  }
  
  while (getline (mesh_file, text_line)) {
    
    /*--- Read the information about inner elements ---*/
    
    position = text_line.find ("NELEM=",0);
    if (position != string::npos) {
      
      /*--- Allocate space for elements ---*/
      elem = new CPrimalGrid*[nElem];
      
      /*--- Set up the global to local element mapping. ---*/
      Global_to_Local_Elem.clear();
      
      if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
        cout << "Distributing elements across all ranks." << endl;
      
      /*--- Loop over all the volumetric elements and store any element that
       contains at least one of an owned node for this rank (i.e., there will
       be element redundancy, since multiple ranks will store the same elems
       on the boundaries of the initial linear partitioning. ---*/
      
      element_count = 0; local_element_count = 0;
      while (element_count < Global_nElem) {
        getline(mesh_file, text_line);
        istringstream elem_line(text_line);
        
        /*--- If this element was marked as required, check type and store. ---*/
        
        map<unsigned long, bool>::const_iterator MI = ElemIn.find(element_count);
        if (MI != ElemIn.end()) {
          
          elem_line >> VTK_Type;
          switch(VTK_Type) {
              
            case TRIANGLE:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_triangle[0];
              elem_line >> vnodes_triangle[1];
              elem_line >> vnodes_triangle[2];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_TRIANGLE; i++) {
                  if (ActDisk_Bool[vnodes_triangle[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_TRIANGLE; j++) {
                      if (vnodes_triangle[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_triangle[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) {
                        vnodes_triangle[i] = ActDiskPoint_Back[vnodes_triangle[i]];
                      }
                      else { vnodes_triangle[i] = vnodes_triangle[i]; }
                    }
                    
                  }
                }
              }

              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count] = local_element_count;
              elem[local_element_count] = new CTriangle(vnodes_triangle[0],
                                                        vnodes_triangle[1],
                                                        vnodes_triangle[2], 2);
              local_element_count++;
              nelem_triangle++;
              break;
              
            case QUADRILATERAL:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_quad[0];
              elem_line >> vnodes_quad[1];
              elem_line >> vnodes_quad[2];
              elem_line >> vnodes_quad[3];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_QUADRILATERAL; i++) {
                  if (ActDisk_Bool[vnodes_quad[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_QUADRILATERAL; j++) {
                      if (vnodes_quad[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_quad[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) {
                        vnodes_quad[i] = ActDiskPoint_Back[vnodes_quad[i]];
                      }
                      else { vnodes_quad[i] = vnodes_quad[i]; }
                    }
                    
                  }
                }
              }
              
              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count] = local_element_count;
              elem[local_element_count] = new CQuadrilateral(vnodes_quad[0],
                                                             vnodes_quad[1],
                                                             vnodes_quad[2],
                                                             vnodes_quad[3], 2);
              local_element_count++;
              nelem_quad++;
              break;
              
            case TETRAHEDRON:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_tetra[0];
              elem_line >> vnodes_tetra[1];
              elem_line >> vnodes_tetra[2];
              elem_line >> vnodes_tetra[3];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_TETRAHEDRON; i++) {
                  if (ActDisk_Bool[vnodes_tetra[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_TETRAHEDRON; j++) {
                      if (vnodes_tetra[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_tetra[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) {
                        vnodes_tetra[i] = ActDiskPoint_Back[vnodes_tetra[i]];
                      }
                      else { vnodes_tetra[i] = vnodes_tetra[i]; }
                    }
                    
                  }
                }
              }
              
              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count] = local_element_count;
              elem[local_element_count] = new CTetrahedron(vnodes_tetra[0],
                                                           vnodes_tetra[1],
                                                           vnodes_tetra[2],
                                                           vnodes_tetra[3]);
              local_element_count++;
              nelem_tetra++;
              break;
              
            case HEXAHEDRON:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_hexa[0];
              elem_line >> vnodes_hexa[1];
              elem_line >> vnodes_hexa[2];
              elem_line >> vnodes_hexa[3];
              elem_line >> vnodes_hexa[4];
              elem_line >> vnodes_hexa[5];
              elem_line >> vnodes_hexa[6];
              elem_line >> vnodes_hexa[7];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_HEXAHEDRON; i++) {
                  if (ActDisk_Bool[vnodes_hexa[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_HEXAHEDRON; j++) {
                      if (vnodes_hexa[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_hexa[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) { vnodes_hexa[i] = ActDiskPoint_Back[vnodes_hexa[i]]; }
                      else { vnodes_hexa[i] = vnodes_hexa[i]; }
                    }
                  }
                }
              }

              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count] = local_element_count;
              elem[local_element_count] = new CHexahedron(vnodes_hexa[0],
                                                          vnodes_hexa[1],
                                                          vnodes_hexa[2],
                                                          vnodes_hexa[3],
                                                          vnodes_hexa[4],
                                                          vnodes_hexa[5],
                                                          vnodes_hexa[6],
                                                          vnodes_hexa[7]);
              local_element_count++;
              nelem_hexa++;
              break;
              
            case PRISM:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_prism[0];
              elem_line >> vnodes_prism[1];
              elem_line >> vnodes_prism[2];
              elem_line >> vnodes_prism[3];
              elem_line >> vnodes_prism[4];
              elem_line >> vnodes_prism[5];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_PRISM; i++) {
                  if (ActDisk_Bool[vnodes_prism[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_PRISM; j++) {
                      if (vnodes_prism[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_prism[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) { vnodes_prism[i] = ActDiskPoint_Back[vnodes_prism[i]]; }
                      else { vnodes_prism[i] = vnodes_prism[i]; }
                    }
                  }
                }
              }

              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count] = local_element_count;
              elem[local_element_count] = new CPrism(vnodes_prism[0],
                                                     vnodes_prism[1],
                                                     vnodes_prism[2],
                                                     vnodes_prism[3],
                                                     vnodes_prism[4],
                                                     vnodes_prism[5]);
              local_element_count++;
              nelem_prism++;
              break;
              
            case PYRAMID:
              
              /*--- Load the connectivity for this element. ---*/
              
              elem_line >> vnodes_pyramid[0];
              elem_line >> vnodes_pyramid[1];
              elem_line >> vnodes_pyramid[2];
              elem_line >> vnodes_pyramid[3];
              elem_line >> vnodes_pyramid[4];
              
              if (actuator_disk) {
                for (unsigned short i = 0; i<N_POINTS_PYRAMID; i++) {
                  if (ActDisk_Bool[vnodes_pyramid[i]]) {
                    
                    Xcg = 0.0; Counter = 0;
                    for (unsigned short j = 0; j<N_POINTS_PYRAMID; j++) {
                      if (vnodes_pyramid[j] < Global_nPoint-ActDiskNewPoints) {
                        Xcg += CoordXVolumePoint[VolumePoint_Inv[vnodes_pyramid[j]]];
                        Counter++;
                      }
                    }
                    Xcg = Xcg / su2double(Counter);
                    
                    if (Counter != 0)  {
                      if (Xcg > Xloc) { vnodes_pyramid[i] = ActDiskPoint_Back[vnodes_pyramid[i]]; }
                      else { vnodes_pyramid[i] = vnodes_pyramid[i]; }
                    }
                  }
                }
              }

              /*--- If any of the nodes were within the linear partition, the
               element is added to our element data structure. ---*/
              
              Global_to_Local_Elem[element_count]=local_element_count;
              elem[local_element_count] = new CPyramid(vnodes_pyramid[0],
                                                       vnodes_pyramid[1],
                                                       vnodes_pyramid[2],
                                                       vnodes_pyramid[3],
                                                       vnodes_pyramid[4]);
              local_element_count++;
              nelem_pyramid++;
              break;
              
          }
        }
        element_count++;
      }
      if (element_count == Global_nElem) break;
    }
  }
  
  mesh_file.close();
  
  /*--- For now, the boundary marker information is still read by the
   master node alone (and eventually distributed by the master as well).
   In the future, this component will also be performed in parallel. ---*/
  
  mesh_file.open(cstr, ios::in);
  
  /*--- If more than one, find the zone in the mesh file ---*/
  

  if (val_nZone > 1 && !harmonic_balance) {
    while (getline (mesh_file,text_line)) {
      /*--- Search for the current domain ---*/
      position = text_line.find ("IZONE=",0);
      if (position != string::npos) {
        text_line.erase (0,6);
        unsigned short jDomain = atoi(text_line.c_str());
        if (jDomain == val_iZone+1) {
          if (rank == MASTER_NODE) cout << "Reading zone " << val_iZone+1 << " markers:" << endl;
          break;
        }
      }
    }
  }
    
    while (getline (mesh_file, text_line)) {
      
      /*--- Read number of markers ---*/
      
      position = text_line.find ("NMARK=",0);
      boundary_marker_count = 0;
      
      if (position != string::npos) {
        text_line.erase (0,6); nMarker = atoi(text_line.c_str());
        
        if (actuator_disk) { nMarker++;  }
        
        if (rank == MASTER_NODE) cout << nMarker << " surface markers." << endl;
        config->SetnMarker_All(nMarker);
        bound = new CPrimalGrid**[nMarker];
        nElem_Bound = new unsigned long [nMarker];
        Tag_to_Marker = new string [nMarker_Max];
        
        bool duplicate = false;
        iMarker=0;
        do {
          
          getline (mesh_file, text_line);
          text_line.erase (0,11);
          string::size_type position;
          
          for (iChar = 0; iChar < 20; iChar++) {
            position = text_line.find( " ", 0 );
            if (position != string::npos) text_line.erase (position,1);
            position = text_line.find( "\r", 0 );
            if (position != string::npos) text_line.erase (position,1);
            position = text_line.find( "\n", 0 );
            if (position != string::npos) text_line.erase (position,1);
          }
          Marker_Tag = text_line.c_str();
          
          duplicate = false;
          if ((actuator_disk) && ( Marker_Tag  == config->GetMarker_ActDiskInlet_TagBound(0))) {
            duplicate = true;
            Marker_Tag_Duplicate  = config->GetMarker_ActDiskOutlet_TagBound(0);
          }
          
          /*--- Physical boundaries definition ---*/
          
          if (Marker_Tag != "SEND_RECEIVE") {
            getline (mesh_file, text_line);
            text_line.erase (0,13); nElem_Bound[iMarker] = atoi(text_line.c_str());
            if (duplicate)  nElem_Bound[iMarker+1]  = nElem_Bound[iMarker];

            if (rank == MASTER_NODE) {
              cout << nElem_Bound[iMarker]  << " boundary elements in index "<< iMarker <<" (Marker = " <<Marker_Tag<< ")." << endl;
              if (duplicate)  cout << nElem_Bound[iMarker+1]  << " boundary elements in index "<< iMarker+1 <<" (Marker = " <<Marker_Tag_Duplicate<< ")." << endl;
            }
            
            /*--- Allocate space for elements ---*/
            
            bound[iMarker] = new CPrimalGrid* [nElem_Bound[iMarker]];
            
            if (duplicate) bound[iMarker+1] = new CPrimalGrid* [nElem_Bound[iMarker+1]];

            nelem_edge_bound = 0; nelem_triangle_bound = 0; nelem_quad_bound = 0; ielem = 0;
            for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
              getline(mesh_file, text_line);
              istringstream bound_line(text_line);
              bound_line >> VTK_Type;
              switch(VTK_Type) {
                case LINE:
                  
                  if (nDim == 3) {
                    cout << "Please remove line boundary conditions from the mesh file!" << endl;
#ifndef HAVE_MPI
                    exit(EXIT_FAILURE);
#else
                    MPI_Barrier(MPI_COMM_WORLD);
                    MPI_Abort(MPI_COMM_WORLD,1);
                    MPI_Finalize();
#endif
                  }
                  
                  bound_line >> vnodes_edge[0]; bound_line >> vnodes_edge[1];
                  bound[iMarker][ielem] = new CLine(vnodes_edge[0], vnodes_edge[1],2);
                  
                  if (duplicate) {
                    if (ActDisk_Bool[vnodes_edge[0]]) { vnodes_edge[0] = ActDiskPoint_Back[vnodes_edge[0]]; }
                    if (ActDisk_Bool[vnodes_edge[1]]) { vnodes_edge[1] = ActDiskPoint_Back[vnodes_edge[1]]; }
                    bound[iMarker+1][ielem] = new CLine(vnodes_edge[0], vnodes_edge[1],2);
                  }
                  
                  ielem++; nelem_edge_bound++; break;
                  
                case TRIANGLE:
                  bound_line >> vnodes_triangle[0]; bound_line >> vnodes_triangle[1]; bound_line >> vnodes_triangle[2];
                  bound[iMarker][ielem] = new CTriangle(vnodes_triangle[0], vnodes_triangle[1], vnodes_triangle[2],3);
                  
                  if (duplicate) {
                    if (ActDisk_Bool[vnodes_triangle[0]]) { vnodes_triangle[0] = ActDiskPoint_Back[vnodes_triangle[0]]; }
                    if (ActDisk_Bool[vnodes_triangle[1]]) { vnodes_triangle[1] = ActDiskPoint_Back[vnodes_triangle[1]]; }
                    if (ActDisk_Bool[vnodes_triangle[2]]) { vnodes_triangle[2] = ActDiskPoint_Back[vnodes_triangle[2]]; }
                    bound[iMarker+1][ielem] = new CTriangle(vnodes_triangle[0], vnodes_triangle[1], vnodes_triangle[2],3);
                    
                  }
                  
                  ielem++; nelem_triangle_bound++; break;
                  
                case QUADRILATERAL:
                  
                  bound_line >> vnodes_quad[0]; bound_line >> vnodes_quad[1]; bound_line >> vnodes_quad[2]; bound_line >> vnodes_quad[3];
                  
                  bound[iMarker][ielem] = new CQuadrilateral(vnodes_quad[0], vnodes_quad[1], vnodes_quad[2], vnodes_quad[3],3);
                  
                  if (duplicate) {
                    if (ActDisk_Bool[vnodes_quad[0]]) { vnodes_quad[0] = ActDiskPoint_Back[vnodes_quad[0]]; }
                    if (ActDisk_Bool[vnodes_quad[1]]) { vnodes_quad[1] = ActDiskPoint_Back[vnodes_quad[1]]; }
                    if (ActDisk_Bool[vnodes_quad[2]]) { vnodes_quad[2] = ActDiskPoint_Back[vnodes_quad[2]]; }
                    if (ActDisk_Bool[vnodes_quad[3]]) { vnodes_quad[3] = ActDiskPoint_Back[vnodes_quad[3]]; }
                    bound[iMarker+1][ielem] = new CQuadrilateral(vnodes_quad[0], vnodes_quad[1], vnodes_quad[2], vnodes_quad[3],3);
                  }
                  
                  ielem++; nelem_quad_bound++;
                  
                  break;
                  
                  
              }
            }
            
            /*--- Update config information storing the boundary information in the right place ---*/
            
            Tag_to_Marker[config->GetMarker_CfgFile_TagBound(Marker_Tag)] = Marker_Tag;
            config->SetMarker_All_TagBound(iMarker, Marker_Tag);
            config->SetMarker_All_KindBC(iMarker, config->GetMarker_CfgFile_KindBC(Marker_Tag));
            config->SetMarker_All_Monitoring(iMarker, config->GetMarker_CfgFile_Monitoring(Marker_Tag));
            config->SetMarker_All_GeoEval(iMarker, config->GetMarker_CfgFile_GeoEval(Marker_Tag));
            config->SetMarker_All_Designing(iMarker, config->GetMarker_CfgFile_Designing(Marker_Tag));
            config->SetMarker_All_Plotting(iMarker, config->GetMarker_CfgFile_Plotting(Marker_Tag));
            config->SetMarker_All_Analyze(iMarker, config->GetMarker_CfgFile_Analyze(Marker_Tag));
            config->SetMarker_All_ZoneInterface(iMarker, config->GetMarker_CfgFile_ZoneInterface(Marker_Tag));
            config->SetMarker_All_DV(iMarker, config->GetMarker_CfgFile_DV(Marker_Tag));
            config->SetMarker_All_Moving(iMarker, config->GetMarker_CfgFile_Moving(Marker_Tag));
            config->SetMarker_All_PerBound(iMarker, config->GetMarker_CfgFile_PerBound(Marker_Tag));
            config->SetMarker_All_SendRecv(iMarker, NONE);
            config->SetMarker_All_Turbomachinery(iMarker, config->GetMarker_CfgFile_Turbomachinery(Marker_Tag));
            config->SetMarker_All_TurbomachineryFlag(iMarker, config->GetMarker_CfgFile_TurbomachineryFlag(Marker_Tag));
            config->SetMarker_All_MixingPlaneInterface(iMarker, config->GetMarker_CfgFile_MixingPlaneInterface(Marker_Tag));
            
            if (duplicate) {
              Tag_to_Marker[config->GetMarker_CfgFile_TagBound(Marker_Tag_Duplicate)] = Marker_Tag_Duplicate;
              config->SetMarker_All_TagBound(iMarker+1, Marker_Tag_Duplicate);
              config->SetMarker_All_KindBC(iMarker+1, config->GetMarker_CfgFile_KindBC(Marker_Tag_Duplicate));
              config->SetMarker_All_Monitoring(iMarker+1, config->GetMarker_CfgFile_Monitoring(Marker_Tag_Duplicate));
              config->SetMarker_All_GeoEval(iMarker+1, config->GetMarker_CfgFile_GeoEval(Marker_Tag_Duplicate));
              config->SetMarker_All_Designing(iMarker+1, config->GetMarker_CfgFile_Designing(Marker_Tag_Duplicate));
              config->SetMarker_All_Plotting(iMarker+1, config->GetMarker_CfgFile_Plotting(Marker_Tag_Duplicate));
              config->SetMarker_All_Analyze(iMarker+1, config->GetMarker_CfgFile_Analyze(Marker_Tag_Duplicate));
              config->SetMarker_All_ZoneInterface(iMarker+1, config->GetMarker_CfgFile_ZoneInterface(Marker_Tag_Duplicate));
              config->SetMarker_All_DV(iMarker+1, config->GetMarker_CfgFile_DV(Marker_Tag_Duplicate));
              config->SetMarker_All_Moving(iMarker+1, config->GetMarker_CfgFile_Moving(Marker_Tag_Duplicate));
              config->SetMarker_All_PerBound(iMarker+1, config->GetMarker_CfgFile_PerBound(Marker_Tag_Duplicate));
              config->SetMarker_All_SendRecv(iMarker+1, NONE);

              boundary_marker_count++;
              iMarker++;
              
            }
            
          }
          
          /*--- Send-Receive boundaries definition ---*/
          
          else {
            
            unsigned long nelem_vertex = 0, vnodes_vertex;
            unsigned short transform;
            getline (mesh_file, text_line);
            text_line.erase (0,13); nElem_Bound[iMarker] = atoi(text_line.c_str());
            bound[iMarker] = new CPrimalGrid* [nElem_Bound[iMarker]];
            
            nelem_vertex = 0; ielem = 0;
            getline (mesh_file, text_line); text_line.erase (0,8);
            config->SetMarker_All_KindBC(iMarker, SEND_RECEIVE);
            config->SetMarker_All_SendRecv(iMarker, atoi(text_line.c_str()));
            
            for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
              getline(mesh_file, text_line);
              istringstream bound_line(text_line);
              bound_line >> VTK_Type; bound_line >> vnodes_vertex; bound_line >> transform;
              
              bound[iMarker][ielem] = new CVertexMPI(vnodes_vertex, nDim);
              bound[iMarker][ielem]->SetRotation_Type(transform);
              ielem++; nelem_vertex++;
            }
            
          }
          
          boundary_marker_count++;
          iMarker++;
          
        } while (iMarker < nMarker);
        
        if (boundary_marker_count == nMarker) break;
        
      }
    }

    while (getline (mesh_file, text_line) && (found_transform == false)) {
      
      /*--- Read periodic transformation info (center, rotation, translation) ---*/
      
      position = text_line.find ("NPERIODIC=",0);
      if (position != string::npos) {
        unsigned short nPeriodic, iPeriodic, iIndex;
        
        /*--- Set bool signifying that periodic transormations were found ---*/
        found_transform = true;
        
        /*--- Read and store the number of transformations. ---*/
        text_line.erase (0,10); nPeriodic = atoi(text_line.c_str());
        if (rank == MASTER_NODE) {
          if (nPeriodic - 1 != 0)
            cout << nPeriodic - 1 << " periodic transformations." << endl;
        }
        config->SetnPeriodicIndex(nPeriodic);
        
        /*--- Store center, rotation, & translation in that order for each. ---*/
        for (iPeriodic = 0; iPeriodic < nPeriodic; iPeriodic++) {
          getline (mesh_file, text_line);
          position = text_line.find ("PERIODIC_INDEX=",0);
          if (position != string::npos) {
            text_line.erase (0,15); iIndex = atoi(text_line.c_str());
            if (iIndex != iPeriodic) {
              cout << "PERIODIC_INDEX out of order in SU2 file!!" << endl;
#ifndef HAVE_MPI
              exit(EXIT_FAILURE);
#else
              MPI_Barrier(MPI_COMM_WORLD);
              MPI_Abort(MPI_COMM_WORLD,1);
              MPI_Finalize();
#endif
            }
          }
          su2double* center    = new su2double[3];
          su2double* rotation  = new su2double[3];
          su2double* translate = new su2double[3];
          getline (mesh_file, text_line);
          istringstream cent(text_line);
          cent >> center[0]; cent >> center[1]; cent >> center[2];
          config->SetPeriodicCenter(iPeriodic, center);
          getline (mesh_file, text_line);
          istringstream rot(text_line);
          rot >> rotation[0]; rot >> rotation[1]; rot >> rotation[2];
          config->SetPeriodicRotation(iPeriodic, rotation);
          getline (mesh_file, text_line);
          istringstream tran(text_line);
          tran >> translate[0]; tran >> translate[1]; tran >> translate[2];
          config->SetPeriodicTranslate(iPeriodic, translate);
          
          delete [] center; delete [] rotation; delete [] translate;
        }
      }
    }
    
    /*--- If no periodic transormations were found, store default zeros ---*/
    
    if (!found_transform) {
      unsigned short nPeriodic = 1, iPeriodic = 0;
      config->SetnPeriodicIndex(nPeriodic);
      su2double* center    = new su2double[3];
      su2double* rotation  = new su2double[3];
      su2double* translate = new su2double[3];
      for (unsigned short iDim = 0; iDim < 3; iDim++) {
        center[iDim] = 0.0; rotation[iDim] = 0.0; translate[iDim] = 0.0;
      }
      config->SetPeriodicCenter(iPeriodic,    center);
      config->SetPeriodicRotation(iPeriodic,  rotation);
      config->SetPeriodicTranslate(iPeriodic, translate);
      delete [] center; delete [] rotation; delete [] translate;
    }
  
  /*--- Close the input file ---*/
  
  mesh_file.close();
  
  /*--- Release actuator disk memory ---*/

  if (actuator_disk) {
    delete [] ActDisk_Bool;
    delete [] ActDiskPoint_Back;
    delete [] VolumePoint_Inv;
    delete [] CoordXVolumePoint;
    delete [] CoordYVolumePoint;
    delete [] CoordZVolumePoint;
    delete [] CoordXActDisk;
    delete [] CoordYActDisk;
    delete [] CoordZActDisk;
  }
  
}

void CPhysicalGeometry::Read_CGNS_Format_Parallel(CConfig *config, string val_mesh_filename, unsigned short val_iZone, unsigned short val_nZone) {
  
  /*--- Original CGNS reader implementation by Thomas D. Economon,
   Francisco Palacios. Improvements for mixed-element meshes generated
   by ICEM added by Martin Spel (3D) & Shlomy Shitrit (2D), April 2014.
   Parallel version by Thomas D. Economon, February 2015. ---*/
  
#ifdef HAVE_CGNS
    
  string text_line, Marker_Tag;
  ifstream mesh_file;
  unsigned short VTK_Type = 0, iMarker = 0;
  unsigned short nMarker_Max = config->GetnMarker_Max();
  unsigned long iPoint = 0, iProcessor = 0, ielem = 0, GlobalIndex = 0;
  unsigned long globalOffset = 0;
  nZone = val_nZone;
  
  /*--- Local variables needed when calling the CGNS mid-level API. ---*/
  
  unsigned long vnodes_cgns[8] = {0,0,0,0,0,0,0,0};
  su2double Coord_cgns[3] = {0.0,0.0,0.0};
  int fn, nbases = 0, nzones = 0, ngrids = 0, ncoords = 0, nsections = 0;
  int *vertices = NULL, *cells = NULL, nMarkers = 0, *boundVerts = NULL, npe;
  int interiorElems = 0, totalVerts = 0;
  int cell_dim = 0, phys_dim = 0, nbndry, parent_flag, file_type;
  char basename[CGNS_STRING_SIZE], zonename[CGNS_STRING_SIZE];
  char coordname[CGNS_STRING_SIZE];
  cgsize_t* cgsize; cgsize = new cgsize_t[3];
  ZoneType_t zonetype;
  DataType_t datatype;
  passivedouble** coordArray = NULL;
  passivedouble*** gridCoords = NULL;
  ElementType_t elemType;
  cgsize_t range_min, range_max, startE, endE;
  range_min = 1;
  string currentElem;
  int** elemTypeVTK = NULL;
  int** elemIndex = NULL;
  int** elemBegin = NULL;
  int** elemEnd = NULL;
  int** nElems = NULL;
  cgsize_t**** connElems = NULL;
  cgsize_t* connElemCGNS = NULL;
  cgsize_t* connElemTemp = NULL;
  cgsize_t ElementDataSize = 0;
  cgsize_t* parentData = NULL;
  int** dataSize = NULL;
  bool** isInternal = NULL;
  char*** sectionNames = NULL;
  
  /*--- Initialize counters for local/global points & elements ---*/
  
  int rank = MASTER_NODE;
  int size = SINGLE_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  unsigned long Local_nElem;
  unsigned long Local_nElemTri, Local_nElemQuad, Local_nElemTet;
  unsigned long Local_nElemHex, Local_nElemPrism, Local_nElemPyramid;
  MPI_Request *send_req, *recv_req;
  MPI_Status  status;
  int ind;
#endif
  
  /*--- Initialize counters for local/global points & elements ---*/
  
  Global_nPoint  = 0; Global_nPointDomain = 0; Global_nElem = 0;
  nelem_edge     = 0; Global_nelem_edge     = 0;
  nelem_triangle = 0; Global_nelem_triangle = 0;
  nelem_quad     = 0; Global_nelem_quad     = 0;
  nelem_tetra    = 0; Global_nelem_tetra    = 0;
  nelem_hexa     = 0; Global_nelem_hexa     = 0;
  nelem_prism    = 0; Global_nelem_prism    = 0;
  nelem_pyramid  = 0; Global_nelem_pyramid  = 0;
  
  /*--- Initialize some additional counters for the parallel partitioning ---*/
  
  unsigned long total_pt_accounted = 0;
  unsigned long rem_points         = 0;
  unsigned long element_count      = 0;
  unsigned long element_remainder  = 0;
  unsigned long total_elems        = 0;
  
  /*--- Allocate memory for the linear partitioning of the mesh. These
   arrays are the size of the number of ranks. ---*/
  
  starting_node = new unsigned long[size];
  ending_node   = new unsigned long[size];
  npoint_procs  = new unsigned long[size];
  
  unsigned long *nPoint_Linear = new unsigned long[size+1];
  unsigned long *nElem_Linear  = new unsigned long[size];
  
  unsigned long *elemB = new unsigned long[size];
  unsigned long *elemE = new unsigned long[size];
  
  unsigned long *elemGlobalID = NULL;
  
  unsigned short *nPoinPerElem = NULL;
  unsigned short *elemTypes = NULL;
  
  bool *isMixed = NULL;
  
  unsigned short connSize = 10;
  
  /*--- Check whether the supplied file is truly a CGNS file. ---*/
  if (cg_is_cgns(val_mesh_filename.c_str(), &file_type) != CG_OK) {
    if (rank == MASTER_NODE) {
    printf( "\n\n   !!! Error !!!\n" );
    printf( " %s is not a CGNS file.\n", val_mesh_filename.c_str());
    printf( " Now exiting...\n\n");
    }
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }
  
  /*--- Open the CGNS file for reading. The value of fn returned
   is the specific index number for this file and will be
   repeatedly used in the function calls. ---*/
  
  if (cg_open(val_mesh_filename.c_str(), CG_MODE_READ, &fn)) cg_error_exit();
  if (rank == MASTER_NODE) {
    cout << "Reading the CGNS file: ";
    cout << val_mesh_filename.c_str() << "." << endl;
  }
  
  /*--- Get the number of databases. This is the highest node
   in the CGNS heirarchy. ---*/
  
  if ( cg_nbases(fn, &nbases) ) cg_error_exit();
  if (rank == MASTER_NODE)
    cout << "CGNS file contains " << nbases << " database(s)." << endl;
  
  /*--- Check if there is more than one database. Throw an
   error if there is because this reader can currently
   only handle one database. ---*/
  
  if ( nbases > 1 ) {
    if (rank == MASTER_NODE) {
    printf("\n\n   !!! Error !!!\n" );
    printf("CGNS reader currently incapable of handling more than 1 database.");
    printf("Now exiting...\n\n");
    }
#ifndef HAVE_MPI
    exit(EXIT_FAILURE);
#else
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Abort(MPI_COMM_WORLD,1);
    MPI_Finalize();
#endif
  }
  
  /*--- Read the databases. Note that the CGNS indexing starts at 1. ---*/
  
  for (int i = 1; i <= nbases; i++) {
    
    if (cg_base_read(fn, i, basename, &cell_dim, &phys_dim)) cg_error_exit();
    
    /*--- Get the number of zones for this base. ---*/
    
    if ( cg_nzones(fn, i, &nzones) ) cg_error_exit();
    if (rank == MASTER_NODE) {
      cout << "Database " << i << ", " << basename << ": " << nzones;
      cout << " zone(s), cell dimension of " << cell_dim << ", physical ";
      cout << "dimension of " << phys_dim << "." << endl;
    }
    
    /*--- Check if there is more than one zone. Throw an
     error if there is, because this reader can currently
     only handle one zone. This could be extended in the future. ---*/
    
    if ( nzones > 1 ) {
      if (rank == MASTER_NODE) {
      printf("\n\n   !!! Error !!!\n" );
      printf("CGNS reader currently incapable of handling more than 1 zone.");
      printf("Now exiting...\n\n");
      }
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
    }
    
    /*--- Initialize some data structures for  all zones. ---*/
    
    vertices     = new int[nzones];
    cells        = new int[nzones];
    boundVerts   = new int[nzones];
    coordArray   = new passivedouble*[nzones];
    gridCoords   = new passivedouble**[nzones];
    elemTypeVTK  = new int*[nzones];
    elemIndex    = new int*[nzones];
    elemBegin    = new int*[nzones];
    elemEnd      = new int*[nzones];
    nElems       = new int*[nzones];
    dataSize     = new int*[nzones];
    isInternal   = new bool*[nzones];
    nMarkers     = 0;
    sectionNames = new char**[nzones];
    connElems    = new cgsize_t***[nzones];
    
    /*--- Loop over all zones in this base. Again, indexing starts at 1. ---*/
    
    for (int j = 1; j <= nzones; j++) {
     
      connElems[j-1] = NULL;
 
      /*--- Read the basic information for this zone, including
       the name and the number of vertices, cells, and
       boundary cells which are stored in the cgsize variable. ---*/
      
      if (cg_zone_read(fn, i, j, zonename, cgsize)) cg_error_exit();
      
      /*--- Rename the zone size information for clarity.
       NOTE: The number of cells here may be only the number of
       interior elements or it may be the total. This needs to
       be counted explicitly later. ---*/
      
      vertices[j-1]   = cgsize[0];
      cells[j-1]      = cgsize[1];
      boundVerts[j-1] = cgsize[2];
      
      /*--- Increment the total number of vertices from all zones. ---*/
      
      nPoint       = vertices[j-1];
      nPointDomain = vertices[j-1];
      
      Global_nPoint       = vertices[j-1];
      Global_nPointDomain = vertices[j-1];
      
      totalVerts += vertices[j-1];
      
      /*--- Print some information about the current zone. ---*/
      
      if (cg_zone_type(fn, i, j, &zonetype)) cg_error_exit();
      if (rank == MASTER_NODE) {
        cout << "Zone " << j << ", " << zonename << ": " << vertices[j-1];
        cout << " vertices, " << cells[j-1] << " cells, " << boundVerts[j-1];
        cout << " boundary vertices." << endl;
      }
      
      /*--- Retrieve the number of grids in this zone. For now, we know
       this is one, but to be more general, this will need to check and
       allow for a loop over all grids. ---*/
      
      if (cg_ngrids(fn, i, j, &ngrids)) cg_error_exit();
      if (ngrids > 1) {
        if (rank == MASTER_NODE) {
          printf("\n\n   !!! Error !!!\n" );
          printf("CGNS reader currently handles only 1 grid per zone.");
          printf("Now exiting...\n\n");
        }
#ifndef HAVE_MPI
        exit(EXIT_FAILURE);
#else
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Abort(MPI_COMM_WORLD,1);
        MPI_Finalize();
#endif
      }
      
      /*--- Check the number of coordinate arrays stored in this zone.
       Should be 2 for 2-D grids and 3 for 3-D grids. ---*/
      
      if (cg_ncoords( fn, i, j, &ncoords)) cg_error_exit();
      if (rank == MASTER_NODE) {
        cout << "Reading grid coordinates." << endl;
        cout << "Number of coordinate dimensions is " << ncoords << "." << endl;
      }
      
      /*--- Compute the number of points that will be on each processor.
       This is a linear partitioning with the addition of a simple load
       balancing for any remainder points. ---*/
      
      total_pt_accounted = 0;
      for (int ii = 0; ii < size; ii++) {
        npoint_procs[ii] = vertices[j-1]/size;
        total_pt_accounted = total_pt_accounted + npoint_procs[ii];
      }
      
      /*--- Get the number of remainder points after the even division ---*/
      
      rem_points = vertices[j-1]-total_pt_accounted;
      for (unsigned long ii = 0; ii < rem_points; ii++) {
        npoint_procs[ii]++;
      }
      
      /*--- Store the local number of nodes and the beginning/end index ---*/
      
      nPoint = npoint_procs[rank];
      starting_node[0] = 0;
      ending_node[0]   = starting_node[0] + npoint_procs[0];
      nPoint_Linear[0] = 0;
      for (int ii = 1; ii < size; ii++) {
        starting_node[ii] = ending_node[ii-1];
        ending_node[ii]   = starting_node[ii] + npoint_procs[ii];
        nPoint_Linear[ii] = nPoint_Linear[ii-1] + npoint_procs[ii-1];
      }
      nPoint_Linear[size] = vertices[j-1];
      
      /*--- Set the value of range_max to the total number of nodes in
       the unstructured mesh. Also allocate memory for the temporary array
       that will hold the grid coordinates as they are extracted. Note the
       +1 for CGNS convention. ---*/
      
      range_min = (cgsize_t)starting_node[rank]+1;
      range_max = (cgsize_t)ending_node[rank];
      coordArray[j-1] = new passivedouble[nPoint];
      
      /*--- Allocate memory for the 2-D array that will store the x, y,
       & z (if required) coordinates for writing into the SU2 mesh. ---*/
      
      gridCoords[j-1] = new passivedouble*[ncoords];
      for (int ii = 0; ii < ncoords; ii++) {
        *(gridCoords[j-1]+ii) = new passivedouble[nPoint];
      }
      
      /*--- Loop over each set of coordinates. Note again
       that the indexing starts at 1. ---*/
      
      for (int k = 1; k <= ncoords; k++) {
        
        /*--- Read the coordinate info. This will retrieve the
         data type (either RealSingle or RealDouble) as
         well as the coordname which will specifiy the
         type of data that it is based in the SIDS convention.
         This might be "CoordinateX," for instance. ---*/
        
        if (cg_coord_info(fn, i, j, k, &datatype, coordname))
          cg_error_exit();
        if (rank == MASTER_NODE) {
          cout << "Loading " << coordname;
          if (size > SINGLE_NODE) {
            cout << " values into linear partitions." << endl;
          } else {
            cout << " values." << endl;
          }
        }
        
        /*--- Always retrieve the grid coords in su2double precision. ---*/
        
        if (datatype != RealDouble) {
          printf("\n\n   !!! Error !!!\n" );
          printf(" CGNS coordinates are not su2double precision.\n");
          printf(" Now exiting...\n\n");
#ifndef HAVE_MPI
          exit(EXIT_FAILURE);
#else
          MPI_Barrier(MPI_COMM_WORLD);
          MPI_Abort(MPI_COMM_WORLD,1);
          MPI_Finalize();
#endif
        }
        if ( cg_coord_read(fn, i, j, coordname, datatype, &range_min,
                           &range_max, coordArray[j-1]) ) cg_error_exit();
        
        /*--- Copy these coords into the array for storage until
         writing the SU2 mesh. ---*/
        
        for (unsigned long m = 0; m < nPoint; m++ ) {
          gridCoords[j-1][k-1][m] = coordArray[j-1][m];
        }
        
      }
      
      /*--- Begin section for retrieving the connectivity info. ---*/
      
      if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
        cout << "Distributing connectivity across all ranks." << endl;
      
      /*--- First check the number of sections. ---*/
      
      if ( cg_nsections(fn, i, j, &nsections) ) cg_error_exit();
      if (rank == MASTER_NODE) {
        cout << "Number of connectivity sections is ";
        cout << nsections << "." << endl;
      }
      
      /*--- Allocate several data structures to hold the various
       pieces of information describing each section. It is
       stored in this manner so that it can be written to
       SU2 memory later. ---*/
      
      elemTypeVTK[j-1] = new int[nsections];
      elemIndex[j-1]   = new int[nsections];
      elemBegin[j-1]   = new int[nsections];
      elemEnd[j-1]     = new int[nsections];
      nElems[j-1]      = new int[nsections];
      dataSize[j-1]    = new int[nsections];
      isInternal[j-1]  = new bool[nsections];
      
      sectionNames[j-1] = new char*[nsections];
      for (int ii = 0; ii < nsections; ii++) {
        sectionNames[j-1][ii]= new char[CGNS_STRING_SIZE];
      }
      
      connElems[j-1] = new cgsize_t**[nsections];
      
      /*--- Loop over each section. This will include the main
       connectivity information for the grid cells, as well
       as any boundaries which were labeled before export. ---*/
      
      for (int s = 1; s <= nsections; s++) {
      
        connElems[j-1][s-1] = NULL; 
        /*--- Read the connectivity details for this section.
         Store the total number of elements in this section
         to be used later for memory allocation. ---*/
        
        if (cg_section_read(fn, i, j, s, sectionNames[j-1][s-1],
                            &elemType, &startE, &endE, &nbndry,
                            &parent_flag)) cg_error_exit();
        
        /*--- Store the beginning and ending index for this section. ---*/
        
        elemBegin[j-1][s-1] = (int)startE;
        elemEnd[j-1][s-1]   = (int)endE;
        
        /*--- Compute element linear partitioning ---*/
        
        element_count = (int) (endE-startE+1);
        total_elems = 0;
        for (int ii = 0; ii < size; ii++) {
          nElem_Linear[ii] = element_count/size;
          total_elems += nElem_Linear[ii];
        }
        
        /*--- Get the number of remainder elements after even division ---*/
        
        element_remainder = element_count-total_elems;
        for (unsigned long ii = 0; ii < element_remainder; ii++) {
          nElem_Linear[ii]++;
        }
        
        /*--- Store the number of elements that this rank is responsible for
         in the current section. ---*/
        
        nElems[j-1][s-1] = (int)nElem_Linear[rank];
        
        /*--- Get starting and end element index for my rank. ---*/
        
        elemB[0] = startE;
        elemE[0] = startE + nElem_Linear[0] - 1;
        for (unsigned long ii = 1; ii < (unsigned long)size; ii++) {
          elemB[ii] = elemE[ii-1]+1;
          elemE[ii] = elemB[ii] + nElem_Linear[ii] - 1;
        }
        
        /*--- Allocate some memory for the handling the connectivity
         and auxiliary data that we are need to communicate. ---*/
        
        connElemCGNS = new cgsize_t[nElems[j-1][s-1]*connSize];
        nPoinPerElem = new unsigned short[nElems[j-1][s-1]];
        elemGlobalID = new unsigned long[nElems[j-1][s-1]];
        elemTypes    = new unsigned short[nElems[j-1][s-1]];
        
        isMixed = new bool[nElems[j-1][s-1]];
        for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) isMixed[ii] = false;

        /*--- Protect against the situation where there are fewer elements
        in a section than number of ranks, or the linear partitioning will
        fail. For now, assume that these must be surfaces, and we will 
        avoid a parallel read and have the master read this section (the
        master processes all of the markers anyway). ---*/

        if (nElems[j-1][s-1] < rank+1) {

          isInternal[j-1][s-1] = false;

        } else {        

        /*--- Retrieve the connectivity information and store. Note that
         we are only accessing our rank's piece of the data here in the
         partial read function in the CGNS API. ---*/

        if (cg_elements_partial_read(fn, i, j, s, (cgsize_t)elemB[rank],
                                    (cgsize_t)elemE[rank], connElemCGNS,
                                    parentData) != CG_OK) cg_error_exit();
        
        /*--- Find the number of nodes required to represent
         this type of element. ---*/
        
        ElementType_t elmt_type;
        if (cg_npe(elemType, &npe)) cg_error_exit();
        
        /*--- Loop through all of the elements in this section to get more
         information and to decide whether it has internal elements. ---*/
        
        int counter = 0;
        for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
          
          /*--- If we have a mixed element section, we need to check the elem
           type one by one. Set the flag to true if mixed. ---*/
          
          if (elemType == MIXED) {
            elmt_type = ElementType_t(connElemCGNS[counter]);
            cg_npe(elmt_type, &npe);
            counter++; for ( int jj = 0; jj < npe; jj++ ) counter++;
            isMixed[ii] = true;
          } else {
            elmt_type = elemType;
          }
          
          /*--- Store the number of verts per elem for the current elem. ---*/
          
          nPoinPerElem[ii] = npe;
          
          /*--- Store the global ID for this element. Note the -1 to move
           from CGNS convention to SU2 convention. We also subtract off
           an additional offset in case we have found boundary sections
           prior to this one, in order to keep the internal element global
           IDs indexed starting from zero. ---*/
          
          elemGlobalID[ii] = elemB[rank] + ii - 1 - globalOffset;
          
          /*--- Need to check the element type and correctly specify the
           VTK identifier for that element. SU2 recognizes elements by
           their VTK number. ---*/
          
          switch (elmt_type) {
            case NODE:
              currentElem   = "Vertex";
              elemTypes[ii] = 1;
              break;
            case BAR_2:
              currentElem   = "Line";
              elemTypes[ii] = 3;
              break;
            case BAR_3:
              currentElem   = "Line";
              elemTypes[ii] = 3;
              break;
            case TRI_3:
              currentElem   = "Triangle";
              elemTypes[ii] = 5;
              break;
            case QUAD_4:
              currentElem   = "Quadrilateral";
              elemTypes[ii] = 9;
              break;
            case TETRA_4:
              currentElem   = "Tetrahedron";
              elemTypes[ii] = 10;
              break;
            case HEXA_8:
              currentElem   = "Hexahedron";
              elemTypes[ii] = 12;
              break;
            case PENTA_6:
              currentElem   = "Prism";
              elemTypes[ii] = 13;
              break;
            case PYRA_5:
              currentElem   = "Pyramid";
              elemTypes[ii] = 14;
              break;
            case HEXA_20:
              if (rank == MASTER_NODE) {
                printf("\n\n   !!! Error !!!\n" );
                printf(" HEXA-20 element type not supported\n");
                printf(" Section %d, npe=%d\n", s, npe);
                printf(" startE %d, endE %d\n", (int)startE, (int)endE);
                printf(" Now exiting...\n\n");
              }
#ifndef HAVE_MPI
              exit(EXIT_FAILURE);
#else
              MPI_Barrier(MPI_COMM_WORLD);
              MPI_Abort(MPI_COMM_WORLD,1);
              MPI_Finalize();
#endif
              break;
            default:
              if (rank == MASTER_NODE) {
                printf("\n\n   !!! Error !!!\n" );
                printf(" Unknown elem: (type %d, npe=%d)\n", elemType, npe);
                printf(" Section %d\n", s);
                printf(" startE %d, endE %d\n", (int)startE, (int)endE);
                printf(" Now exiting...\n\n");
              }
#ifndef HAVE_MPI
              exit(EXIT_FAILURE);
#else
              MPI_Abort(MPI_COMM_WORLD,1);
              MPI_Finalize();
#endif
              break;
          }
          
          /*--- Check if the elements in this section are part
           of the internal domain or are part of the boundary
           surfaces. This will be used to separate the
           internal connectivity from the boundary connectivity.
           We will check for quad and tri elements for 3-D meshes
           because these will be the boundaries. Similarly, line
           elements will be boundaries to 2-D problems. ---*/
          
          if ( cell_dim == 2 ) {
            
            /*--- In 2-D check for line elements, VTK type 3. ---*/
            
            if (elemTypes[ii] == 3) {
              isInternal[j-1][s-1] = false;
            } else {
              isInternal[j-1][s-1] = true;
              interiorElems++;
            }
            
          } else if (cell_dim == 3) {
            
            /*--- In 3-D check for tri/quad elements, VTK types 5 or 9. ---*/
            
            switch (elemTypes[ii]) {
              case 5:
              case 9:
                isInternal[j-1][s-1] = false;
                break;
              default:
                isInternal[j-1][s-1] = true;
                interiorElems++;
                break;
            }
            
          }
        }
        
        /*--- Print some information to the console. ---*/
        
        if (rank == MASTER_NODE) {
          for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ )
            if (isMixed[ii]) {currentElem = "Mixed"; break;}
          cout << "Loading section " << sectionNames[j-1][s-1];
          cout << " of element type " << currentElem << "." << endl;
        }
       
        } 
        
         /*--- If we have found that this is a boundary section (we assume
         that internal cells and boundary cells do not exist in the same
         section together), the master node reads the boundary section.
         Otherwise, we have all ranks read and communicate the internals. ---*/
        
        if (!isInternal[j-1][s-1]) {
          
          /*--- Master node should read this entire marker section. Free
           the memory for the conn. from the CGNS file since we are going
           to read the section again with the master. ---*/
          
          delete [] connElemCGNS;
          delete [] nPoinPerElem;
          delete [] elemTypes;
          delete [] elemGlobalID;
          delete [] isMixed;
          
          /*--- Since we found an internal section, we should adjust the
           element global ID offset by the total size of the section. ---*/
          
          globalOffset += element_count;
          
          if (rank == MASTER_NODE) {
            
            /*--- First increment the markers ---*/
            
            nMarkers++;
            
            /*--- Read the section info again ---*/
            
            if ( cg_section_read(fn, i, j, s, sectionNames[j-1][s-1],
                                 &elemType, &startE, &endE, &nbndry,
                                 &parent_flag) ) cg_error_exit();
            
            /*--- Store the number of elems (all on the master). ---*/
            
            nElems[j-1][s-1] = (int) (endE-startE+1);
            
            /*--- Read and store the total amount of data that will be
             listed when reading this section. ---*/
            
            if (cg_ElementDataSize(fn, i, j, s, &ElementDataSize))
              cg_error_exit();
            dataSize[j-1][s-1] = ElementDataSize;
            
            /*--- Find the number of nodes required to represent
             this type of element. ---*/
            
            if (cg_npe(elemType, &npe)) cg_error_exit();
            elemIndex[j-1][s-1] = npe;
            
            /*--- Need to check the element type and correctly
             specify the VTK identifier for that element.
             SU2 recognizes elements by their VTK number. ---*/
            
            switch (elemType) {
              case NODE:
                elemTypeVTK[j-1][s-1] = 1;
                break;
              case BAR_2:
                elemTypeVTK[j-1][s-1] = 3;
                break;
              case BAR_3:
                elemTypeVTK[j-1][s-1] = 3;
                break;
              case TRI_3:
                elemTypeVTK[j-1][s-1] = 5;
                break;
              case QUAD_4:
                elemTypeVTK[j-1][s-1] = 9;
                break;
              case TETRA_4:
                elemTypeVTK[j-1][s-1] = 10;
                break;
              case HEXA_8:
                elemTypeVTK[j-1][s-1] = 12;
                break;
              case PENTA_6:
                elemTypeVTK[j-1][s-1] = 13;
                break;
              case PYRA_5:
                elemTypeVTK[j-1][s-1] = 14;
                break;
              case HEXA_20:
                printf( "\n\n   !!! Error !!!\n" );
                printf( " HEXA-20 element type not supported\n");
                printf(" Section %d, npe=%d\n", s, npe);
                printf(" startE %d, endE %d\n", (int)startE, (int)endE);
                printf( " Now exiting...\n\n");
#ifndef HAVE_MPI
                exit(EXIT_FAILURE);
#else
                MPI_Barrier(MPI_COMM_WORLD);
                MPI_Abort(MPI_COMM_WORLD,1);
                MPI_Finalize();
#endif
                break;
              case MIXED:
                currentElem = "Mixed";
                elemTypeVTK[j-1][s-1] = -1;
                break;
              default:
                printf( "\n\n   !!! Error !!!\n" );
                printf( " Unknown elem: (type %d, npe=%d)\n", elemType, npe);
                printf(" Section %d\n", s);
                printf(" startE %d, endE %d\n", (int)startE, (int)endE);
                printf( " Now exiting...\n\n");
#ifndef HAVE_MPI
                exit(EXIT_FAILURE);
#else
                MPI_Abort(MPI_COMM_WORLD,1);
                MPI_Finalize();
#endif
                break;
            }
            
            /*--- In case of mixed data type, allocate place for 8 nodes
             maximum (hex), plus element type. ---*/
            
            if (elemTypeVTK[j-1][s-1] == -1) elemIndex[j-1][s-1] = 9;
            
            /*--- Allocate memory for accessing the connectivity and to
             store it in the proper data structure for post-processing. ---*/
            
            connElemTemp = new cgsize_t[dataSize[j-1][s-1]];
            connElems[j-1][s-1] = new cgsize_t*[elemIndex[j-1][s-1]];
            for (int jj = 0; jj < elemIndex[j-1][s-1]; jj++) {
              connElems[j-1][s-1][jj] = new cgsize_t[nElems[j-1][s-1]];
            }
            
            /*--- Retrieve the connectivity information and store. ---*/
            
            if (cg_elements_read(fn, i, j, s, connElemTemp, parentData))
              cg_error_exit();
            
            /*--- Copy these values into the larger array for
             storage until writing the SU2 file. ---*/
            
            if (elemTypeVTK[j-1][s-1] == -1) {
              int counter = 0;
              for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
                ElementType_t elmt_type = ElementType_t(connElemTemp[counter]);
                cg_npe( elmt_type, &npe);
                counter++;
                connElems[j-1][s-1][0][ii] = elmt_type;
                for ( int jj = 0; jj < npe; jj++ ) {
                  connElems[j-1][s-1][jj+1][ii] = connElemTemp[counter] - 1;
                  counter++;
                }
              }
            } else {
              int counter = 0;
              for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
                for ( int jj = 0; jj < elemIndex[j-1][s-1]; jj++ ) {
                  connElems[j-1][s-1][jj][ii] = connElemTemp[counter] - 1;
                  counter++;
                }
              }
            }
            delete[] connElemTemp;
          
          } // end master
          
        } else {
          
          /*--- These are internal elems. Allocate memory on each proc. ---*/
          
          connElemTemp = new cgsize_t[nElems[j-1][s-1]*connSize];
          
           /*--- Copy these values into the larger array for
           storage until writing the SU2 file. ---*/

          int counterTemp = 0, counterCGNS = 0;
          for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
            
            /*--- Store the conn in chunks of connSize for simplicity. ---*/
            
            counterTemp = ii*connSize;
            
            /*--- Store the connectivity values. Note we subtract one from
             the CGNS 1-based convention. We may also need to remove the first
             entry is this is a mixed element section. ---*/
            
            if (isMixed[ii]) counterCGNS++;
            for ( int jj = 0; jj < nPoinPerElem[ii]; jj++) {
              connElemTemp[counterTemp] = connElemCGNS[counterCGNS + jj] - 1;
              counterTemp++;
            }
            counterCGNS += nPoinPerElem[ii];
            
          }

          /*--- Free the memory for the conn. from the CGNS file. ---*/
          
          delete [] connElemCGNS;
          delete [] isMixed;
          
          /*--- We now have the connectivity stored in linearly partitioned
           chunks. We need to loop through and decide how many elements we
           must send to each rank in order to have all elements that
           surround a particular "owned" node on each rank (i.e., elements
           will appear on multiple ranks). First, initialize a counter
           and flag. ---*/
          
          int *nElem_Send = new int[size+1]; nElem_Send[0] = 0;
          int *nElem_Recv = new int[size+1]; nElem_Recv[0] = 0;
          int *nElem_Flag = new int[size];
          
          for (int ii=0; ii < size; ii++) {
            nElem_Send[ii] = 0;
            nElem_Recv[ii] = 0;
            nElem_Flag[ii]= -1;
          }
          nElem_Send[size] = 0; nElem_Recv[size] = 0;
          
          for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
            for ( int jj = 0; jj < nPoinPerElem[ii]; jj++ ) {
              
              /*--- Get the index of the current point. ---*/
              
              iPoint = connElemTemp[ii*connSize + jj];
              
              /*--- Search for the processor that owns this point ---*/
              
              iProcessor = iPoint/npoint_procs[0];
              if (iProcessor >= (unsigned long)size) iProcessor = (unsigned long)size-1;
              if (iPoint >= nPoint_Linear[iProcessor])
                while(iPoint >= nPoint_Linear[iProcessor+1]) iProcessor++;
              else
                while(iPoint <  nPoint_Linear[iProcessor])   iProcessor--;
              
              /*--- If we have not visted this element yet, increment our
               number of elements that must be sent to a particular proc. ---*/
              
              if (nElem_Flag[iProcessor] != ii) {
                nElem_Flag[iProcessor] = ii;
                nElem_Send[iProcessor+1]++;
              }
              
            }
          }
          
          /*--- Communicate the number of cells to be sent/recv'd amongst
           all processors. After this communication, each proc knows how
           many cells it will receive from each other processor. ---*/
          
#ifdef HAVE_MPI
          MPI_Alltoall(&(nElem_Send[1]), 1, MPI_INT,
                       &(nElem_Recv[1]), 1, MPI_INT, MPI_COMM_WORLD);
#else
          nElem_Recv[1] = nElem_Send[1];
#endif
          
          /*--- Prepare to send connectivities. First check how many
           messages we will be sending and receiving. Here we also put
           the counters into cumulative storage format to make the
           communications simpler. ---*/
          
          int nSends = 0, nRecvs = 0;
          for (int ii=0; ii < size; ii++) nElem_Flag[ii] = -1;
          
          for (int ii = 0; ii < size; ii++) {
            
            if ((ii != rank) && (nElem_Send[ii+1] > 0)) nSends++;
            if ((ii != rank) && (nElem_Recv[ii+1] > 0)) nRecvs++;
            
            nElem_Send[ii+1] += nElem_Send[ii];
            nElem_Recv[ii+1] += nElem_Recv[ii];
          }

          /*--- Allocate memory to hold the connectivity that we are
           sending. Note that we are also sending the VTK element type
           in the first position and also the global ID. We have assumed
           a constant message size of a hex element + 2 extra vals. ---*/
          
          unsigned long *connSend = NULL;
          connSend = new unsigned long[connSize*nElem_Send[size]];
          for (int ii = 0; ii < connSize*nElem_Send[size]; ii++)
            connSend[ii] = 0;
          
          /*--- Create an index variable to keep track of our index
           position as we load up the send buffer. ---*/
          
          unsigned long *index = new unsigned long[size];
          for (int ii=0; ii < size; ii++) index[ii] = connSize*nElem_Send[ii];
          
          /*--- Loop through our elements and load the elems and their
           additional data that we will send to the other procs. ---*/
          
          for ( int ii = 0; ii < nElems[j-1][s-1]; ii++ ) {
            for ( int jj = 0; jj < nPoinPerElem[ii]; jj++ ) {
              
              /*--- Get the index of the current point. ---*/
              
              iPoint = connElemTemp[ii*connSize + jj];
              
              /*--- Search for the processor that owns this point ---*/
              
              iProcessor = iPoint/npoint_procs[0];
              if (iProcessor >= (unsigned long)size) iProcessor = (unsigned long)size-1;
              if (iPoint >= nPoint_Linear[iProcessor])
                while(iPoint >= nPoint_Linear[iProcessor+1]) iProcessor++;
              else
                while(iPoint <  nPoint_Linear[iProcessor])   iProcessor--;
              
              /*--- Load connectivity into the buffer for sending ---*/
              
              if (nElem_Flag[iProcessor] != ii) {
                
                nElem_Flag[iProcessor] = ii;
                unsigned long nn = index[iProcessor];
                
                /*--- Load the VTK type first into the conn array,
                 then the connectivity vals, and last, the global ID. ---*/
                
                connSend[nn] = elemTypes[ii]; nn++;
                for ( int kk = 0; kk < nPoinPerElem[ii]; kk++ ) {
                  connSend[nn] = connElemTemp[ii*connSize + kk]; nn++;
                }
                connSend[nn] = (cgsize_t)elemGlobalID[ii];
                
                /*--- Increment the index by the message length ---*/
                
                index[iProcessor] += connSize;
                
              }
            }
          }

          /*--- Free memory after loading up the send buffer. ---*/
          
          delete [] connElemTemp;
          delete [] elemTypes;
          delete [] nPoinPerElem;
          delete [] elemGlobalID;
          delete [] index;
          
          /*--- Allocate the memory that we need for receiving the conn
           values and then cue up the non-blocking receives. Note that
           we do not include our own rank in the communications. We will
           directly copy our own data later. ---*/
          
          unsigned long *connRecv = NULL;
          connRecv = new unsigned long[connSize*nElem_Recv[size]];
          for (int ii = 0; ii < connSize*nElem_Recv[size]; ii++)
            connRecv[ii] = 0;
            
#ifdef HAVE_MPI
          send_req = new MPI_Request[nSends];
          recv_req = new MPI_Request[nRecvs];
          unsigned long iMessage = 0;
          for (int ii=0; ii<size; ii++) {
            if ((ii != rank) && (nElem_Recv[ii+1] > nElem_Recv[ii])) {
              int ll     = connSize*nElem_Recv[ii];
              int kk     = nElem_Recv[ii+1] - nElem_Recv[ii];
              int count  = connSize*kk;
              int source = ii;
              int tag    = ii + 1;
              SU2_MPI::Irecv(&(connRecv[ll]), count, MPI_UNSIGNED_LONG, source, tag,
                        MPI_COMM_WORLD, &(recv_req[iMessage]));
              iMessage++;
            }
          }
          
          /*--- Launch the non-blocking sends of the connectivity. ---*/
          
          iMessage = 0;
          for (int ii=0; ii<size; ii++) {
            if ((ii != rank) && (nElem_Send[ii+1] > nElem_Send[ii])) {
              int ll = connSize*nElem_Send[ii];
              int kk = nElem_Send[ii+1] - nElem_Send[ii];
              int count  = connSize*kk;
              int dest = ii;
              int tag    = rank + 1;
              SU2_MPI::Isend(&(connSend[ll]), count, MPI_UNSIGNED_LONG, dest, tag,
                        MPI_COMM_WORLD, &(send_req[iMessage]));
              iMessage++;
            }
          }
#endif
          
          /*--- Copy my own rank's data into the recv buffer directly. ---*/
          
          int mm = connSize*nElem_Recv[rank];
          int ll = connSize*nElem_Send[rank];
          int kk = connSize*nElem_Send[rank+1];

          for (int nn=ll; nn<kk; nn++, mm++) connRecv[mm] = connSend[nn];
          
          /*--- Wait for the non-blocking sends and recvs to complete ---*/
          
#ifdef HAVE_MPI
          int number = nSends;
          for (int ii = 0; ii < nSends; ii++)
            SU2_MPI::Waitany(number, send_req, &ind, &status);
          
          number = nRecvs;
          for (int ii = 0; ii < nRecvs; ii++)
            SU2_MPI::Waitany(number, recv_req, &ind, &status);
          
          delete [] send_req;
          delete [] recv_req;
#endif
          
          /*--- Store the connectivity for this rank in the proper data
           structure before post-processing below. First, allocate the
           appropriate amount of memory for this section. ---*/

          connElems[j-1][s-1] = new cgsize_t*[connSize];
          for (int jj = 0; jj < connSize; jj++) {
            connElems[j-1][s-1][jj] = new cgsize_t[nElem_Recv[size]];
          }
          for (int ii = 0; ii < nElem_Recv[size]; ii++) {
            for (int jj = 0; jj < connSize; jj++) {
              connElems[j-1][s-1][jj][ii] = (cgsize_t)connRecv[ii*connSize+jj];
            }
          }
          
          /*--- Store the total number of elements I now have for
           the current section after completing the communications. ---*/
          
          nElems[j-1][s-1] = nElem_Recv[size];

          /*--- Free temporary memory from communications ---*/
          
          delete [] connSend;
          delete [] connRecv;
          delete [] nElem_Recv;
          delete [] nElem_Send;
          delete [] nElem_Flag;
          
        }
        
      } // end section
      
    } // end zone
    
  } // end database
  
  /*--- Close the CGNS file. ---*/
  
  if ( cg_close(fn) ) cg_error_exit();
  if (rank == MASTER_NODE)
    cout << "Successfully closed the CGNS file." << endl;
  
  /*--- Load the data from the CGNS file into SU2 memory. ---*/
  
  if (rank == MASTER_NODE)
    cout << endl << "Loading CGNS data into SU2 data structures." << endl;
  
  /*--- Read the dimension of the problem ---*/
  
  nDim = cell_dim;
  if (rank == MASTER_NODE) {
    if (nDim == 2) cout << "Two dimensional problem." << endl;
    if (nDim == 3) cout << "Three dimensional problem." << endl;
  }
  
  /*--- Initialize an array for the adjacency information (ParMETIS). ---*/
  
  vector< vector<unsigned long> > adj_nodes(nPoint, vector<unsigned long>(0));
  
  /*--- Loop to check total number of elements we have locally. ---*/
  
  ielem = 0;
  for (int k = 0; k < nzones; k++) {
    for (int s = 0; s < nsections; s++) {
      if (isInternal[k][s]) {
        for ( int i = 0; i < nElems[k][s]; i++) {
          ielem++;
        }
      }
    }
  }
  nElem = ielem;
  
  /*--- Store the total number of interior elements (global). ---*/
  
#ifdef HAVE_MPI
  Local_nElem = interiorElems;
  SU2_MPI::Allreduce(&Local_nElem, &Global_nElem, 1, MPI_UNSIGNED_LONG,
                MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nElem = interiorElems;
  nElem        = Global_nElem;
#endif
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE)) {
    cout << Global_nElem << " interior elements before linear partitioning." << endl;
  } else if (rank == MASTER_NODE) {
    cout << Global_nElem << " interior elements." << endl;
  }
  
  /*--- Set up the global to local element mapping. ---*/
  Global_to_Local_Elem.clear();
  
  /*--- Allocate space for elements. We allocate enough for all interior
   elements globally, but we will only instantiate our local set. ---*/
  
  elem = new CPrimalGrid*[nElem];
  ielem = 0;
  unsigned long global_id = 0;
  
  /*--- Loop over all the internal, local volumetric elements. ---*/
  
  for (int k = 0; k < nzones; k++) {
    for (int s = 0; s < nsections; s++) {
      if (isInternal[k][s]) {
        for ( int i = 0; i < nElems[k][s]; i++) {
          
          /*--- Get the VTK type for this element. This is stored in the
           first entry of the connectivity structure. ---*/
          
          VTK_Type = connElems[k][s][0][i];
          
          /*--- Instantiate this element and build adjacency structure. ---*/
          
          switch(VTK_Type) {
              
            case TRIANGLE:
              
              for ( unsigned short j = 0; j < N_POINTS_TRIANGLE; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_TRIANGLE+1][i];
              for (unsigned short ii=0; ii<N_POINTS_TRIANGLE; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  for (unsigned short j=0; j<N_POINTS_TRIANGLE; j++) {
                    if (ii!=j) {
                      adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[j]);
                    }
                  }
                }
              }
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CTriangle(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], nDim);
              ielem++; nelem_triangle++;
              break;
              
            case QUADRILATERAL:
              
              for ( unsigned short j = 0; j < N_POINTS_QUADRILATERAL; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_QUADRILATERAL+1][i];
              
              for (unsigned short ii=0; ii<N_POINTS_QUADRILATERAL; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  
                  /*--- Build adjacency assuming the VTK connectivity ---*/
                  
                  adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+1)%4]);
                  adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+3)%4]);
                  
                }
              }
              
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CQuadrilateral(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3], nDim);
              ielem++; nelem_quad++;
              break;
              
            case TETRAHEDRON:
              
              for ( unsigned short j = 0; j < N_POINTS_TETRAHEDRON; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_TETRAHEDRON+1][i];
              for (unsigned short ii=0; ii<N_POINTS_TETRAHEDRON; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  for (unsigned short j=0; j<N_POINTS_TETRAHEDRON; j++) {
                    if (ii!=j) {
                      adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[j]);
                    }
                  }
                }
              }
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CTetrahedron(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3]);
              ielem++; nelem_tetra++;
              break;
              
            case HEXAHEDRON:
              
              for ( unsigned short j = 0; j < N_POINTS_HEXAHEDRON; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_HEXAHEDRON+1][i];
              
              for (unsigned short ii=0; ii<N_POINTS_HEXAHEDRON; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  
                  /*--- Build adjacency assuming the VTK connectivity ---*/
                  
                  if (ii < 4) {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+1)%4]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+3)%4]);
                  } else {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii-3)%4+4]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii-1)%4+4]);
                  }
                  adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+4)%8]);

                }
              }
              
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CHexahedron(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3], vnodes_cgns[4], vnodes_cgns[5], vnodes_cgns[6], vnodes_cgns[7]);
              ielem++; nelem_hexa++;
              break;
              
            case PRISM:
              
              for ( unsigned short j = 0; j < N_POINTS_PRISM; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_PRISM+1][i];
              
              for (unsigned short ii=0; ii<N_POINTS_PRISM; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  
                  /*--- Build adjacency assuming the VTK connectivity ---*/
                  
                  if (ii < 3) {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+1)%3]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+2)%3]);
                  } else {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii-2)%3+3]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii-1)%3+3]);
                  }
                  adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+3)%6]);
                  
                }
              }
              
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CPrism(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3], vnodes_cgns[4], vnodes_cgns[5]);
              ielem++; nelem_prism++;
              break;
              
            case PYRAMID:
              
              for ( unsigned short j = 0; j < N_POINTS_PYRAMID; j++ ) {
                vnodes_cgns[j] = connElems[k][s][j+1][i];
              }
              global_id = connElems[k][s][N_POINTS_PYRAMID+1][i];
              
              for (unsigned short ii=0; ii<N_POINTS_PYRAMID; ii++) {
                if ((vnodes_cgns[ii]>=starting_node[rank])&&(vnodes_cgns[ii]<ending_node[rank])) {
                  
                  /*--- Build adjacency assuming the VTK connectivity ---*/
                  
                  if (ii < 4) {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+1)%4]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[(ii+3)%4]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[4]);
                  } else {
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[0]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[1]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[2]);
                    adj_nodes[vnodes_cgns[ii]-starting_node[rank]].push_back(vnodes_cgns[3]);
                  }
                  
                }
              }
              
              Global_to_Local_Elem[global_id]=ielem;
              elem[ielem] = new CPyramid(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3], vnodes_cgns[4]);
              ielem++; nelem_pyramid++;
              break;
              
            default:
              if (rank == MASTER_NODE)
                cout << "Element type not suppported!" << endl;
#ifndef HAVE_MPI
              exit(EXIT_FAILURE);
#else
              MPI_Barrier(MPI_COMM_WORLD);
              MPI_Abort(MPI_COMM_WORLD,1);
              MPI_Finalize();
#endif
              break;
          }
        }
      }
    }
  }
  
#ifdef HAVE_MPI
  Local_nElemTri     = nelem_triangle;
  Local_nElemQuad    = nelem_quad;
  Local_nElemTet     = nelem_tetra;
  Local_nElemHex     = nelem_hexa;
  Local_nElemPrism   = nelem_prism;
  Local_nElemPyramid = nelem_pyramid;
  SU2_MPI::Allreduce(&Local_nElemTri,     &Global_nelem_triangle,  1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemQuad,    &Global_nelem_quad,      1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemTet,     &Global_nelem_tetra,     1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemHex,     &Global_nelem_hexa,      1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemPrism,   &Global_nelem_prism,     1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nElemPyramid, &Global_nelem_pyramid,   1,
                MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nelem_triangle = nelem_triangle;
  Global_nelem_quad     = nelem_quad;
  Global_nelem_tetra    = nelem_tetra;
  Global_nelem_hexa     = nelem_hexa;
  Global_nelem_prism    = nelem_prism;
  Global_nelem_pyramid  = nelem_pyramid;
#endif

#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
  
  /*--- Post process the adjacency information in order to get it into the
   proper format before sending the data to ParMETIS. We need to remove
   repeats and adjust the size of the array for each local node. ---*/
  
  if ((rank == MASTER_NODE) && (size > SINGLE_NODE))
    cout << "Building the graph adjacency structure." << endl;
  
  unsigned long loc_adjc_size=0;
  vector<unsigned long> adjac_vec;
  unsigned long adj_elem_size;
  vector<unsigned long>::iterator it;
  
  xadj = new idx_t[npoint_procs[rank]+1];
  xadj[0]=0;
  vector<unsigned long> temp_adjacency;
  unsigned long local_count=0;
  
  for (unsigned long i = 0; i < nPoint; i++) {
    
    for (unsigned long j=0; j<adj_nodes[i].size(); j++) {
      temp_adjacency.push_back(adj_nodes[i][j]);
    }
    
    sort(temp_adjacency.begin(), temp_adjacency.end());
    it = unique( temp_adjacency.begin(), temp_adjacency.end());
    loc_adjc_size=it - temp_adjacency.begin();
    
    temp_adjacency.resize( loc_adjc_size);
    xadj[local_count+1]=xadj[local_count]+loc_adjc_size;
    local_count++;
    
    for (unsigned long j=0; j<loc_adjc_size; j++) {
      adjac_vec.push_back(temp_adjacency[j]);
    }
    temp_adjacency.clear();
    adj_nodes[i].clear();
  }
  
  /*--- Now that we know the size, create the final adjacency array ---*/
  
  adj_elem_size = xadj[npoint_procs[rank]];
  adjacency = new idx_t [adj_elem_size];
  copy(adjac_vec.begin(), adjac_vec.end(), adjacency);
  
  xadj_size = npoint_procs[rank]+1;
  adjacency_size = adj_elem_size;
  
  /*--- Free temporary memory used to build the adjacency. ---*/
  
  adjac_vec.clear();
  
#endif
#endif
  
  adj_nodes.clear();
  
  /*--- Store the nodal coordinates from the linear partitioning. ---*/

  if ((rank == MASTER_NODE) && (size > SINGLE_NODE)) {
    cout << Global_nPoint << " grid points before linear partitioning." << endl;
  } else if (rank == MASTER_NODE) {
    cout << Global_nPoint << " grid points." << endl;
  }
  
  iPoint = 0;
  nPointNode = nPoint;
  node = new CPoint*[nPoint];
  GlobalIndex = starting_node[rank];
  for (int k = 0; k < nzones; k++ ) {
    for (unsigned long i = 0; i < nPoint; i++ ) {
      for (int j = 0; j < cell_dim; j++ ) Coord_cgns[j] = gridCoords[k][j][i];
      switch(nDim) {
        case 2:
          node[iPoint] = new CPoint(Coord_cgns[0], Coord_cgns[1], GlobalIndex, config);
          iPoint++; break;
        case 3:
          node[iPoint] = new CPoint(Coord_cgns[0], Coord_cgns[1], Coord_cgns[2], GlobalIndex, config);
          iPoint++; break;
      }
      GlobalIndex++;
    }
  }
  
  /*--- For now, the master node takes care of all markers. ---*/
  
  if (rank == MASTER_NODE) {
    
    /*--- Read number of markers ---*/

    nMarker = nMarkers;
    cout << nMarker << " surface markers." << endl;
    config->SetnMarker_All(nMarker);
    bound = new CPrimalGrid**[nMarker];
    nElem_Bound = new unsigned long [nMarker];
    Tag_to_Marker = new string [nMarker_Max];
    
    iMarker = 0;
    for ( int k = 0; k < nzones; k ++ ) {
      for ( int s = 0; s < nsections; s++ ) {
        if ( !isInternal[k][s] ) {
          
          /*--- Initialize some counter variables ---*/
          
          nelem_edge_bound = 0; nelem_triangle_bound = 0;
          nelem_quad_bound = 0; ielem = 0;
          
          Marker_Tag = sectionNames[k][s];
          
          /*--- Remove whitespaces from the marker names ---*/
          Marker_Tag.erase(remove(Marker_Tag.begin(), Marker_Tag.end(),' '), Marker_Tag.end());
          
          if (Marker_Tag != "SEND_RECEIVE") {
            nElem_Bound[iMarker] = nElems[k][s];
            if (rank == MASTER_NODE) {
              cout << nElem_Bound[iMarker]  << " boundary elements in index ";
              cout << iMarker <<" (Marker = " <<Marker_Tag<< ")." << endl;
            }
            bound[iMarker] = new CPrimalGrid*[nElem_Bound[iMarker]];
            
            for ( int i = 0; i < nElems[k][s]; i++ ) {
              
              /*--- Get the VTK type for this element. Check for mixed
               elements. ---*/
              
              if (elemTypeVTK[k][s] == -1 ) {
                
                /*--- Mixed-element support. Check the elem type. ---*/
                
                ElementType_t elmt_type = ElementType_t(connElems[k][s][0][i]);
                cg_npe( elmt_type, &npe);
                
                switch (elmt_type) {
                  case NODE:    VTK_Type = 1;  break;
                  case BAR_2:   VTK_Type = 3;  break;
                  case BAR_3:   VTK_Type = 3;  break;
                  case TRI_3:   VTK_Type = 5;  break;
                  case QUAD_4:  VTK_Type = 9;  break;
                  case TETRA_4: VTK_Type = 10; break;
                  case HEXA_8:  VTK_Type = 12; break;
                  case PENTA_6: VTK_Type = 13; break;
                  case PYRA_5:  VTK_Type = 14; break;
                  default:
                    cout << "Kind of element not suppported!" << endl;
#ifndef HAVE_MPI
                    exit(EXIT_FAILURE);
#else
                    MPI_Barrier(MPI_COMM_WORLD);
                    MPI_Abort(MPI_COMM_WORLD,1);
                    MPI_Finalize();
#endif
                    break;
                }
                
                /*--- Transfer the nodes for this element. ---*/
                
                for ( int j = 1; j < npe+1; j++ ) {
                  vnodes_cgns[j-1] = connElems[k][s][j][i];
                }
                
              } else {
                
                /*--- Not a mixed section. We know the element type. ---*/
                
                VTK_Type = elemTypeVTK[k][s];
                
                /*--- Transfer the nodes for this element. ---*/
                
                for ( int j = 0; j < elemIndex[k][s]; j++ ) {
                  vnodes_cgns[j] = connElems[k][s][j][i];
                }
                
              }
              
              /*--- Instantiate the boundary elements. ---*/
              
              switch(VTK_Type) {
                case LINE:
                  if (nDim == 3) {
                    if (rank == MASTER_NODE) {
                      printf( "\n\n   !!! Error !!!\n" );
                      printf( "Remove line boundary elems from the mesh.");
                    }
#ifndef HAVE_MPI
                    exit(EXIT_FAILURE);
#else
                    MPI_Barrier(MPI_COMM_WORLD);
                    MPI_Abort(MPI_COMM_WORLD,1);
                    MPI_Finalize();
#endif
                  }
                  bound[iMarker][ielem] = new CLine(vnodes_cgns[0], vnodes_cgns[1],2);
                  ielem++; nelem_edge_bound++; break;
                case TRIANGLE:
                  bound[iMarker][ielem] = new CTriangle(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2],3);
                  ielem++; nelem_triangle_bound++; break;
                case QUADRILATERAL:
                  bound[iMarker][ielem] = new CQuadrilateral(vnodes_cgns[0], vnodes_cgns[1], vnodes_cgns[2], vnodes_cgns[3],3);
                  ielem++; nelem_quad_bound++; break;
              }
            }
            
            /*--- Update config information storing the boundary information in the right place ---*/
            
            Tag_to_Marker[config->GetMarker_CfgFile_TagBound(Marker_Tag)] = Marker_Tag;
            config->SetMarker_All_TagBound(iMarker, Marker_Tag);
            config->SetMarker_All_KindBC(iMarker, config->GetMarker_CfgFile_KindBC(Marker_Tag));
            config->SetMarker_All_Monitoring(iMarker, config->GetMarker_CfgFile_Monitoring(Marker_Tag));
            config->SetMarker_All_GeoEval(iMarker, config->GetMarker_CfgFile_GeoEval(Marker_Tag));
            config->SetMarker_All_Designing(iMarker, config->GetMarker_CfgFile_Designing(Marker_Tag));
            config->SetMarker_All_Plotting(iMarker, config->GetMarker_CfgFile_Plotting(Marker_Tag));
            config->SetMarker_All_Analyze(iMarker, config->GetMarker_CfgFile_Analyze(Marker_Tag));
            config->SetMarker_All_ZoneInterface(iMarker, config->GetMarker_CfgFile_ZoneInterface(Marker_Tag));
            config->SetMarker_All_DV(iMarker, config->GetMarker_CfgFile_DV(Marker_Tag));
            config->SetMarker_All_Moving(iMarker, config->GetMarker_CfgFile_Moving(Marker_Tag));
            config->SetMarker_All_PerBound(iMarker, config->GetMarker_CfgFile_PerBound(Marker_Tag));
            config->SetMarker_All_SendRecv(iMarker, NONE);
            config->SetMarker_All_Turbomachinery(iMarker, config->GetMarker_CfgFile_Turbomachinery(Marker_Tag));
            config->SetMarker_All_TurbomachineryFlag(iMarker, config->GetMarker_CfgFile_TurbomachineryFlag(Marker_Tag));
            config->SetMarker_All_MixingPlaneInterface(iMarker, config->GetMarker_CfgFile_MixingPlaneInterface(Marker_Tag));
            
          }
          iMarker++;
        }
      }
    }
    
    /*--- Periodic transormations is not implement, store default zeros ---*/
    unsigned short nPeriodic = 1, iPeriodic = 0;
    config->SetnPeriodicIndex(nPeriodic);
    su2double* center    = new su2double[3];
    su2double* rotation  = new su2double[3];
    su2double* translate = new su2double[3];
    for (unsigned short iDim = 0; iDim < 3; iDim++) {
      center[iDim] = 0.0; rotation[iDim] = 0.0; translate[iDim] = 0.0;
    }
    config->SetPeriodicCenter(iPeriodic, center);
    config->SetPeriodicRotation(iPeriodic, rotation);
    config->SetPeriodicTranslate(iPeriodic, translate);
    delete [] center; delete [] rotation; delete [] translate;
    
  }
  
  /*--- Deallocate temporary memory. ---*/
  
  delete[] vertices;
  delete[] cells;
  delete[] boundVerts;
  
  for ( int kk = 0; kk < nzones; kk++) {
    for (int ii = 0; ii < nsections; ii++) {
      if (isInternal[kk][ii]) {
        for (int jj = 0; jj < connSize; jj++) {
          if (connElems[kk][ii][jj] != NULL) delete [] connElems[kk][ii][jj];
        }
        if (connElems[kk][ii] != NULL) delete []  connElems[kk][ii];
      } else if (!isInternal[kk][ii] && rank == MASTER_NODE) {
        for (int jj = 0; jj < elemIndex[kk][ii]; jj++) {
          if (connElems[kk][ii][jj] != NULL) delete [] connElems[kk][ii][jj];
        }
        if (connElems[kk][ii] != NULL) delete [] connElems[kk][ii];
      }
    }
    if (connElems[kk] != NULL) delete [] connElems[kk];
  }
  if (connElems != NULL) delete[] connElems;
  
  for ( int j = 0; j < nzones; j++) {
    delete [] coordArray[j];
    delete [] elemTypeVTK[j];
    delete [] elemIndex[j];
    delete [] nElems[j];
    delete [] dataSize[j];
    delete [] isInternal[j];
    delete [] elemBegin[j];
    delete [] elemEnd[j];
    for (int ii = 0; ii < nsections; ii++) {
      delete[] sectionNames[j][ii];
    }
    delete[] sectionNames[j];
  }
  
  delete [] coordArray;
  delete [] elemTypeVTK;
  delete [] elemIndex;
  delete [] nElems;
  delete [] dataSize;
  delete [] isInternal;
  delete [] sectionNames;
  delete [] elemBegin;
  delete [] elemEnd;
  
  for ( int j = 0; j < nzones; j++) {
    for ( int i = 0; i < ncoords; i++ ) {
      delete [] gridCoords[j][i];
    }
    delete [] gridCoords[j];
  }
  delete [] gridCoords;
  
  delete [] nPoint_Linear;
  delete [] nElem_Linear;
  
  delete [] elemB;
  delete [] elemE;
  
  delete [] cgsize;

#else
  cout << "SU2 built without CGNS support!!" << endl;
  cout << "To use CGNS, remove the -DNO_CGNS directive ";
  cout << "from the makefile and supply the correct path";
  cout << " to the CGNS library." << endl;
  exit(EXIT_FAILURE);
#endif
  
}

void CPhysicalGeometry::Check_IntElem_Orientation(CConfig *config) {
  
  unsigned long Point_1, Point_2, Point_3, Point_4, Point_5, Point_6,
  iElem, triangle_flip = 0, quad_flip = 0, tet_flip = 0, prism_flip = 0,
  hexa_flip = 0, pyram_flip = 0;
  su2double test_1, test_2, test_3, test_4, *Coord_1, *Coord_2, *Coord_3, *Coord_4,
  *Coord_5, *Coord_6, a[3] = {0.0,0.0,0.0}, b[3] = {0.0,0.0,0.0}, c[3] = {0.0,0.0,0.0}, n[3] = {0.0,0.0,0.0}, test;
  unsigned short iDim;
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  /*--- Loop over all the elements ---*/
  
  for (iElem = 0; iElem < nElem; iElem++) {
    
    /*--- 2D grid, triangle case ---*/
    
    if (elem[iElem]->GetVTK_Type() == TRIANGLE) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]); }
      test = a[0]*b[1]-b[0]*a[1];
      
      if (test < 0.0) {
    	  elem[iElem]->Change_Orientation();
    	  triangle_flip++;
      }
    }
    
    /*--- 2D grid, quadrilateral case ---*/
    
    if (elem[iElem]->GetVTK_Type() == QUADRILATERAL) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(3); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]); }
      test_1 = a[0]*b[1]-b[0]*a[1];
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_3[iDim]-Coord_2[iDim]);
        b[iDim] = 0.5*(Coord_4[iDim]-Coord_2[iDim]); }
      test_2 = a[0]*b[1]-b[0]*a[1];
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_4[iDim]-Coord_3[iDim]);
        b[iDim] = 0.5*(Coord_1[iDim]-Coord_3[iDim]); }
      test_3 = a[0]*b[1]-b[0]*a[1];
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_1[iDim]-Coord_4[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_4[iDim]); }
      test_4 = a[0]*b[1]-b[0]*a[1];
      
      if ((test_1 < 0.0) && (test_2 < 0.0) && (test_3 < 0.0) && (test_4 < 0.0)) {
        elem[iElem]->Change_Orientation();
        quad_flip++;
      }
    }
    
    /*--- 3D grid, tetrahedron case ---*/
    
    if (elem[iElem]->GetVTK_Type() == TETRAHEDRON) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(3); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      if (test < 0.0) {
    	  elem[iElem]->Change_Orientation();
    	  tet_flip++;
      }
      
    }
    
    /*--- 3D grid, prism case ---*/
    
    if (elem[iElem]->GetVTK_Type() == PRISM) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(3); Coord_4 = node[Point_4]->GetCoord();
      Point_5 = elem[iElem]->GetNode(4); Coord_5 = node[Point_5]->GetCoord();
      Point_6 = elem[iElem]->GetNode(5); Coord_6 = node[Point_6]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        c[iDim] = (Coord_4[iDim]-Coord_1[iDim])+
        (Coord_5[iDim]-Coord_2[iDim])+
        (Coord_6[iDim]-Coord_3[iDim]); }
      
      /*--- The normal vector should point to the interior of the element ---*/
      
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_1 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_5[iDim]-Coord_4[iDim]);
        b[iDim] = 0.5*(Coord_6[iDim]-Coord_4[iDim]);
        c[iDim] = (Coord_1[iDim]-Coord_4[iDim])+
        (Coord_2[iDim]-Coord_5[iDim])+
        (Coord_3[iDim]-Coord_6[iDim]); }
      
      /*--- The normal vector should point to the interior of the element ---*/
      
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_2 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      if ((test_1 < 0.0) || (test_2 < 0.0)) {
          elem[iElem]->Change_Orientation();
          prism_flip++;
      }
      
    }
    
    if (elem[iElem]->GetVTK_Type() == HEXAHEDRON) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(5); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_1 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      Point_1 = elem[iElem]->GetNode(2); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(3); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(0); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(7); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_2 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      Point_1 = elem[iElem]->GetNode(1); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(2); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(3); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(6); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_3 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      Point_1 = elem[iElem]->GetNode(3); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(0); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(1); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(4); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_4 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      if ((test_1 < 0.0) || (test_2 < 0.0) || (test_3 < 0.0)
          || (test_4 < 0.0)) {
    	  elem[iElem]->Change_Orientation();
      	  hexa_flip++;
      }
      
    }
    
    if (elem[iElem]->GetVTK_Type() == PYRAMID) {
      
      Point_1 = elem[iElem]->GetNode(0); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(4); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_1 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      Point_1 = elem[iElem]->GetNode(2); Coord_1 = node[Point_1]->GetCoord();
      Point_2 = elem[iElem]->GetNode(3); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(0); Coord_3 = node[Point_3]->GetCoord();
      Point_4 = elem[iElem]->GetNode(4); Coord_4 = node[Point_4]->GetCoord();
      
      for (iDim = 0; iDim < nDim; iDim++) {
        a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
        b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
        c[iDim] = Coord_4[iDim]-Coord_1[iDim]; }
      n[0] = a[1]*b[2]-b[1]*a[2];
      n[1] = -(a[0]*b[2]-b[0]*a[2]);
      n[2] = a[0]*b[1]-b[0]*a[1];
      
      test_2 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
      
      if ((test_1 < 0.0) || (test_2 < 0.0)) {
          elem[iElem]->Change_Orientation();
      	  pyram_flip++;
      }
      
    }
    
  }
  
#ifdef HAVE_MPI
  unsigned long Mytriangle_flip  = triangle_flip;
  unsigned long Myquad_flip      = quad_flip;
  unsigned long Mytet_flip       = tet_flip;
  unsigned long Myprism_flip     = prism_flip;
  unsigned long Myhexa_flip      = hexa_flip;
  unsigned long Mypyram_flip     = pyram_flip;

  SU2_MPI::Allreduce(&Mytriangle_flip, &triangle_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Myquad_flip, &quad_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Mytet_flip, &tet_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Myprism_flip, &prism_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Myhexa_flip, &hexa_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Mypyram_flip, &pyram_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#endif

  if (rank == MASTER_NODE) {
  	if (triangle_flip > 0) cout << "There has been a re-orientation of the TRIANGLE volume elements." << endl;
  	if (quad_flip > 0) cout << "There has been a re-orientation of the QUADRILATERAL volume elements." << endl;
  	if (tet_flip > 0) cout << "There has been a re-orientation of the TETRAHEDRON volume elements." << endl;
  	if (prism_flip > 0) cout << "There has been a re-orientation of the PRISM volume elements." << endl;
  	if (hexa_flip > 0) cout << "There has been a re-orientation of the HEXAHEDRON volume elements." << endl;
  	if (pyram_flip > 0) cout << "There has been a re-orientation of the PYRAMID volume elements." << endl;
  }

}

void CPhysicalGeometry::Check_BoundElem_Orientation(CConfig *config) {
  
  unsigned long Point_1_Surface, Point_2_Surface, Point_3_Surface, Point_4_Surface,
  iElem_Domain, Point_Domain = 0, Point_Surface, iElem_Surface,
  line_flip = 0, triangle_flip = 0, quad_flip = 0;
  su2double test_1, test_2, test_3, test_4, *Coord_1, *Coord_2, *Coord_3, *Coord_4,
  *Coord_5, a[3] = {0.0,0.0,0.0}, b[3] = {0.0,0.0,0.0}, c[3] = {0.0,0.0,0.0}, n[3] = {0.0,0.0,0.0}, test;
  unsigned short iDim, iMarker, iNode_Domain, iNode_Surface;
  bool find;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    
    if (config->GetMarker_All_KindBC(iMarker) != INTERNAL_BOUNDARY) {
      
      for (iElem_Surface = 0; iElem_Surface < nElem_Bound[iMarker]; iElem_Surface++) {
        
        iElem_Domain = bound[iMarker][iElem_Surface]->GetDomainElement();
        for (iNode_Domain = 0; iNode_Domain < elem[iElem_Domain]->GetnNodes(); iNode_Domain++) {
          Point_Domain = elem[iElem_Domain]->GetNode(iNode_Domain);
          find = false;
          for (iNode_Surface = 0; iNode_Surface < bound[iMarker][iElem_Surface]->GetnNodes(); iNode_Surface++) {
            Point_Surface = bound[iMarker][iElem_Surface]->GetNode(iNode_Surface);
            if (Point_Surface == Point_Domain) {find = true; break;}
          }
          if (!find) break;
        }
        
        /*--- 2D grid, line case ---*/
        
        if (bound[iMarker][iElem_Surface]->GetVTK_Type() == LINE) {

          Point_1_Surface = bound[iMarker][iElem_Surface]->GetNode(0); Coord_1 = node[Point_1_Surface]->GetCoord();
          Point_2_Surface = bound[iMarker][iElem_Surface]->GetNode(1); Coord_2 = node[Point_2_Surface]->GetCoord();
          Coord_3 = node[Point_Domain]->GetCoord();

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
            b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
          }
          test = a[0]*b[1]-b[0]*a[1];

          if (test < 0.0) {
            bound[iMarker][iElem_Surface]->Change_Orientation();
            node[Point_1_Surface]->SetFlip_Orientation();
            node[Point_2_Surface]->SetFlip_Orientation();
            line_flip++;
          }

        }
        
        /*--- 3D grid, triangle case ---*/
        
        if (bound[iMarker][iElem_Surface]->GetVTK_Type() == TRIANGLE) {

          Point_1_Surface = bound[iMarker][iElem_Surface]->GetNode(0); Coord_1 = node[Point_1_Surface]->GetCoord();
          Point_2_Surface = bound[iMarker][iElem_Surface]->GetNode(1); Coord_2 = node[Point_2_Surface]->GetCoord();
          Point_3_Surface = bound[iMarker][iElem_Surface]->GetNode(2); Coord_3 = node[Point_3_Surface]->GetCoord();
          Coord_4 = node[Point_Domain]->GetCoord();

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
            b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
            c[iDim] = Coord_4[iDim]-Coord_1[iDim];
          }
          n[0] = a[1]*b[2]-b[1]*a[2];
          n[1] = -(a[0]*b[2]-b[0]*a[2]);
          n[2] = a[0]*b[1]-b[0]*a[1];

          test = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];
          if (test < 0.0) {
            bound[iMarker][iElem_Surface]->Change_Orientation();
            node[Point_1_Surface]->SetFlip_Orientation();
            node[Point_2_Surface]->SetFlip_Orientation();
            node[Point_3_Surface]->SetFlip_Orientation();
            triangle_flip++;
          }

        }
        
        /*--- 3D grid, rectangle case ---*/
        
        if (bound[iMarker][iElem_Surface]->GetVTK_Type() == QUADRILATERAL) {

          Point_1_Surface = bound[iMarker][iElem_Surface]->GetNode(0); Coord_1 = node[Point_1_Surface]->GetCoord();
          Point_2_Surface = bound[iMarker][iElem_Surface]->GetNode(1); Coord_2 = node[Point_2_Surface]->GetCoord();
          Point_3_Surface = bound[iMarker][iElem_Surface]->GetNode(2); Coord_3 = node[Point_3_Surface]->GetCoord();
          Point_4_Surface = bound[iMarker][iElem_Surface]->GetNode(3); Coord_4 = node[Point_4_Surface]->GetCoord();
          Coord_5 = node[Point_Domain]->GetCoord();

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_2[iDim]-Coord_1[iDim]);
            b[iDim] = 0.5*(Coord_3[iDim]-Coord_1[iDim]);
            c[iDim] = Coord_5[iDim]-Coord_1[iDim];
          }
          n[0] = a[1]*b[2]-b[1]*a[2];
          n[1] = -(a[0]*b[2]-b[0]*a[2]);
          n[2] = a[0]*b[1]-b[0]*a[1];
          test_1 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_3[iDim]-Coord_2[iDim]);
            b[iDim] = 0.5*(Coord_4[iDim]-Coord_2[iDim]);
            c[iDim] = Coord_5[iDim]-Coord_2[iDim];
          }
          n[0] = a[1]*b[2]-b[1]*a[2];
          n[1] = -(a[0]*b[2]-b[0]*a[2]);
          n[2] = a[0]*b[1]-b[0]*a[1];
          test_2 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_4[iDim]-Coord_3[iDim]);
            b[iDim] = 0.5*(Coord_1[iDim]-Coord_3[iDim]);
            c[iDim] = Coord_5[iDim]-Coord_3[iDim];
          }
          n[0] = a[1]*b[2]-b[1]*a[2];
          n[1] = -(a[0]*b[2]-b[0]*a[2]);
          n[2] = a[0]*b[1]-b[0]*a[1];
          test_3 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];

          for (iDim = 0; iDim < nDim; iDim++) {
            a[iDim] = 0.5*(Coord_1[iDim]-Coord_4[iDim]);
            b[iDim] = 0.5*(Coord_3[iDim]-Coord_4[iDim]);
            c[iDim] = Coord_5[iDim]-Coord_4[iDim];
          }
          n[0] = a[1]*b[2]-b[1]*a[2];
          n[1] = -(a[0]*b[2]-b[0]*a[2]);
          n[2] = a[0]*b[1]-b[0]*a[1];
          test_4 = n[0]*c[0]+n[1]*c[1]+n[2]*c[2];

          if ((test_1 < 0.0) && (test_2 < 0.0) && (test_3 < 0.0) && (test_4 < 0.0)) {
            bound[iMarker][iElem_Surface]->Change_Orientation();
            node[Point_1_Surface]->SetFlip_Orientation();
            node[Point_2_Surface]->SetFlip_Orientation();
            node[Point_3_Surface]->SetFlip_Orientation();
            node[Point_4_Surface]->SetFlip_Orientation();
            quad_flip++;
          }

        }
      }
    }
  }

#ifdef HAVE_MPI
  unsigned long Myline_flip   = line_flip;
  unsigned long Mytriangle_flip  = triangle_flip;
  unsigned long Myquad_flip   = quad_flip;
  SU2_MPI::Allreduce(&Myline_flip, &line_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Mytriangle_flip, &triangle_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Myquad_flip, &quad_flip, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#endif

  if (rank == MASTER_NODE) {
	if (line_flip > 0) cout << "There has been a re-orientation of the LINE surface elements." << endl;
  	if (triangle_flip > 0) cout << "There has been a re-orientation of the TRIANGLE surface elements." << endl;
  	if (quad_flip > 0) cout << "There has been a re-orientation of the QUADRILATERAL surface elements." << endl;
  }

}

void CPhysicalGeometry::ComputeWall_Distance(CConfig *config) {

  /*--- Compute the total number of nodes on no-slip boundaries ---*/

  unsigned long nVertex_SolidWall = 0;
  for(unsigned short iMarker=0; iMarker<config->GetnMarker_All(); ++iMarker) {
    if( (config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)  ||
       (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL) ) {
      nVertex_SolidWall += GetnVertex(iMarker);
    }
  }

  /*--- Allocate the vectors to hold boundary node coordinates
   and its local ID. ---*/

  vector<su2double>     Coord_bound(nDim*nVertex_SolidWall);
  vector<unsigned long> PointIDs(nVertex_SolidWall);

  /*--- Retrieve and store the coordinates of the no-slip boundary nodes
   and their local point IDs. ---*/

  unsigned long ii = 0, jj = 0;
  for(unsigned short iMarker=0; iMarker<config->GetnMarker_All(); ++iMarker) {
    if( (config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)  ||
       (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL) ) {
      for(unsigned long iVertex=0; iVertex<GetnVertex(iMarker); ++iVertex) {
        unsigned long iPoint = vertex[iMarker][iVertex]->GetNode();
        PointIDs[jj++] = iPoint;
        for(unsigned short iDim=0; iDim<nDim; ++iDim)
          Coord_bound[ii++] = node[iPoint]->GetCoord(iDim);
      }
    }
  }

  /*--- Build the ADT of the boundary nodes. ---*/

  su2_adtPointsOnlyClass WallADT(nDim, nVertex_SolidWall, Coord_bound.data(), PointIDs.data());

  /*--- Loop over all interior mesh nodes and compute the distances to each
   of the no-slip boundary nodes. Store the minimum distance to the wall
   for each interior mesh node. ---*/

  if( WallADT.IsEmpty() ) {
  
    /*--- No solid wall boundary nodes in the entire mesh.
     Set the wall distance to zero for all nodes. ---*/
    
    for(unsigned long iPoint=0; iPoint<GetnPoint(); ++iPoint)
      node[iPoint]->SetWall_Distance(0.0);
  }
  else {

    /*--- Solid wall boundary nodes are present. Compute the wall
     distance for all nodes. ---*/
    
    for(unsigned long iPoint=0; iPoint<GetnPoint(); ++iPoint) {
      
      su2double dist;
      unsigned long pointID;
      int rankID;
      
      WallADT.DetermineNearestNode(node[iPoint]->GetCoord(), dist,
                                   pointID, rankID);
      node[iPoint]->SetWall_Distance(dist);
    }
  }
  
}

void CPhysicalGeometry::SetPositive_ZArea(CConfig *config) {
  unsigned short iMarker, Boundary, Monitoring;
  unsigned long iVertex, iPoint;
  su2double *Normal, PositiveXArea, PositiveYArea, PositiveZArea, WettedArea, CoordX = 0.0, CoordY = 0.0, CoordZ = 0.0, MinCoordX = 1E10, MinCoordY = 1E10,
  MinCoordZ = 1E10, MaxCoordX = -1E10, MaxCoordY = -1E10, MaxCoordZ = -1E10, TotalMinCoordX = 1E10, TotalMinCoordY = 1E10,
  TotalMinCoordZ = 1E10, TotalMaxCoordX = -1E10, TotalMaxCoordY = -1E10, TotalMaxCoordZ = -1E10;
  su2double TotalPositiveXArea = 0.0, TotalPositiveYArea = 0.0, TotalPositiveZArea = 0.0, TotalWettedArea = 0.0, AxiFactor;

  bool axisymmetric = config->GetAxisymmetric();

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  PositiveXArea = 0.0;
  PositiveYArea = 0.0;
  PositiveZArea = 0.0;
  WettedArea = 0.0;

  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    Boundary = config->GetMarker_All_KindBC(iMarker);
    Monitoring = config->GetMarker_All_Monitoring(iMarker);
    
    if (((Boundary == EULER_WALL)              ||
         (Boundary == HEAT_FLUX)               ||
         (Boundary == ISOTHERMAL)              ||
         (Boundary == LOAD_BOUNDARY)           ||
         (Boundary == DISPLACEMENT_BOUNDARY)) && (Monitoring == YES))

      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();

        if (node[iPoint]->GetDomain()) {
          Normal = vertex[iMarker][iVertex]->GetNormal();
          CoordX = node[iPoint]->GetCoord(0);
          CoordY = node[iPoint]->GetCoord(1);
          if (nDim == 3) CoordZ = node[iPoint]->GetCoord(2);

          if (axisymmetric) AxiFactor = 2.0*PI_NUMBER*node[iPoint]->GetCoord(1);
          else AxiFactor = 1.0;

          if (nDim == 2) WettedArea += AxiFactor * sqrt (Normal[0]*Normal[0] + Normal[1]*Normal[1]);
          if (nDim == 3) WettedArea += sqrt (Normal[0]*Normal[0] + Normal[1]*Normal[1] + Normal[2]*Normal[2]);

          if (Normal[0] < 0) PositiveXArea -= Normal[0];
          if (Normal[1] < 0) PositiveYArea -= Normal[1];
          if ((nDim == 3) && (Normal[2] < 0)) PositiveZArea -= Normal[2];
          
          if (CoordX < MinCoordX) MinCoordX = CoordX;
          if (CoordX > MaxCoordX) MaxCoordX = CoordX;

          if (CoordY < MinCoordY) MinCoordY = CoordY;
          if (CoordY > MaxCoordY) MaxCoordY = CoordY;

          if (nDim == 3) {
            if (CoordZ < MinCoordZ) MinCoordZ = CoordZ;
            if (CoordZ > MaxCoordZ) MaxCoordZ = CoordZ;
          }
          
        }
      }
  }
  
#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&PositiveXArea, &TotalPositiveXArea, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&PositiveYArea, &TotalPositiveYArea, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&PositiveZArea, &TotalPositiveZArea, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  
  SU2_MPI::Allreduce(&MinCoordX, &TotalMinCoordX, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MinCoordY, &TotalMinCoordY, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MinCoordZ, &TotalMinCoordZ, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  
  SU2_MPI::Allreduce(&MaxCoordX, &TotalMaxCoordX, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MaxCoordY, &TotalMaxCoordY, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&MaxCoordZ, &TotalMaxCoordZ, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

  SU2_MPI::Allreduce(&WettedArea, &TotalWettedArea, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#else
  TotalPositiveXArea = PositiveXArea;
  TotalPositiveYArea = PositiveYArea;
  TotalPositiveZArea = PositiveZArea;
  
  TotalMinCoordX = MinCoordX;
  TotalMinCoordY = MinCoordY;
  TotalMinCoordZ = MinCoordZ;

  TotalMaxCoordX = MaxCoordX;
  TotalMaxCoordY = MaxCoordY;
  TotalMaxCoordZ = MaxCoordZ;

  TotalWettedArea    = WettedArea;
#endif
    
  if (config->GetRefArea() == 0.0) {
  	if (nDim == 3) config->SetRefArea(TotalPositiveZArea);
  	else config->SetRefArea(TotalPositiveYArea);
  }
  
  if (rank == MASTER_NODE) {

  	cout << "Wetted area = "<< TotalWettedArea;
    if ((nDim == 3) || (axisymmetric)) { if (config->GetSystemMeasurements() == SI) cout <<" m^2." << endl; else cout <<" ft^2." << endl; }
    else { if (config->GetSystemMeasurements() == SI) cout <<" m." << endl; else cout <<" ft." << endl; }

    cout << "Area projection in the x-plane = "<< TotalPositiveXArea;
    if (nDim == 3) { if (config->GetSystemMeasurements() == SI) cout <<" m^2,"; else cout <<" ft^2,"; }
    else { if (config->GetSystemMeasurements() == SI) cout <<" m,"; else cout <<" ft,"; }

    cout << " y-plane = "<< TotalPositiveYArea;
    if (nDim == 3) { if (config->GetSystemMeasurements() == SI) cout <<" m^2,"; else cout <<" ft^2,"; }
    else { if (config->GetSystemMeasurements() == SI) cout <<" m." << endl; else cout <<" ft." << endl; }

    if (nDim == 3) { cout << " z-plane = "<< TotalPositiveZArea;
      if (config->GetSystemMeasurements() == SI) cout <<" m^2." << endl; else cout <<" ft^2."<< endl; }
    
    cout << "Max. coordinate in the x-direction = "<< TotalMaxCoordX;
    if (config->GetSystemMeasurements() == SI) cout <<" m,"; else cout <<" ft,";
    
    cout << " y-direction = "<< TotalMaxCoordY;
    if (config->GetSystemMeasurements() == SI) cout <<" m"; else cout <<" ft";
    
    if (nDim == 3) {
    	cout << ", z-direction = "<< TotalMaxCoordZ;
      if (config->GetSystemMeasurements() == SI) cout <<" m." << endl; else cout <<" ft."<< endl;
    }
    else cout << "." << endl;
    
    cout << "Min coordinate in the x-direction = "<< TotalMinCoordX;
    if (config->GetSystemMeasurements() == SI) cout <<" m,"; else cout <<" ft";
    
    cout << " y-direction = "<< TotalMinCoordY;
    if (config->GetSystemMeasurements() == SI) cout <<" m,"; else cout <<" ft";
    
    if (nDim == 3) {
    	cout << ", z-direction = "<< TotalMinCoordZ;
      if (config->GetSystemMeasurements() == SI) cout <<" m." << endl; else cout <<" ft."<< endl;
    }
    else cout << "." << endl;

  }
  
}

void CPhysicalGeometry::SetPoint_Connectivity(void) {
  
  unsigned short Node_Neighbor, iNode, iNeighbor;
  unsigned long jElem, Point_Neighbor, iPoint, iElem;
  
  /*--- Loop over all the elements ---*/
  
  for (iElem = 0; iElem < nElem; iElem++)
    
  /*--- Loop over all the nodes of an element ---*/
    
    for (iNode = 0; iNode < elem[iElem]->GetnNodes(); iNode++) {
      iPoint = elem[iElem]->GetNode(iNode);
      
      /*--- Store the element into the point ---*/
      
      node[iPoint]->SetElem(iElem);
    }

  /*--- Loop over all the points ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    
  /*--- Loop over all elements shared by the point ---*/
    
    for (iElem = 0; iElem < node[iPoint]->GetnElem(); iElem++) {
      
      jElem = node[iPoint]->GetElem(iElem);
      
      /*--- If we find the point iPoint in the surronding element ---*/
      
      for (iNode = 0; iNode < elem[jElem]->GetnNodes(); iNode++)
        
        if (elem[jElem]->GetNode(iNode) == iPoint)
          
        /*--- Localize the local index of the neighbor of iPoint in the element ---*/
          
          for (iNeighbor = 0; iNeighbor < elem[jElem]->GetnNeighbor_Nodes(iNode); iNeighbor++) {
            Node_Neighbor = elem[jElem]->GetNeighbor_Nodes(iNode, iNeighbor);
            Point_Neighbor = elem[jElem]->GetNode(Node_Neighbor);
            
            /*--- Store the point into the point ---*/
            
            node[iPoint]->SetPoint(Point_Neighbor);
          }
    }
  
  /*--- Set the number of neighbors variable, this is
   important for JST and multigrid in parallel ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    node[iPoint]->SetnNeighbor(node[iPoint]->GetnPoint());
  
}

void CPhysicalGeometry::SetRCM_Ordering(CConfig *config) {
  unsigned long iPoint, AdjPoint, AuxPoint, AddPoint, iElem, iNode, jNode;
  vector<unsigned long> Queue, AuxQueue, Result;
  unsigned short Degree, MinDegree, iDim, iMarker;
  bool *inQueue;
  
  inQueue = new bool [nPoint];
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    inQueue[iPoint] = false;
  
  /*--- Select the node with the lowest degree in the grid. ---*/
  
  MinDegree = node[0]->GetnNeighbor(); AddPoint = 0;
  for (iPoint = 1; iPoint < nPointDomain; iPoint++) {
    Degree = node[iPoint]->GetnPoint();
    if (Degree < MinDegree) { MinDegree = Degree; AddPoint = iPoint; }
  }
  
  /*--- Add the node in the first free position. ---*/
  
  Result.push_back(AddPoint); inQueue[AddPoint] = true;
  
  /*--- Loop until reorganize all the nodes ---*/
  
  do {
    
    /*--- Add to the queue all the nodes adjacent in the increasing
     order of their degree, checking if the element is already
     in the Queue. ---*/
    
    AuxQueue.clear();
    for (iNode = 0; iNode < node[AddPoint]->GetnPoint(); iNode++) {
      AdjPoint = node[AddPoint]->GetPoint(iNode);
      if ((!inQueue[AdjPoint]) && (AdjPoint < nPointDomain)) {
        AuxQueue.push_back(AdjPoint);
      }
    }
    
    if (AuxQueue.size() != 0) {
      
      /*--- Sort the auxiliar queue based on the number of neighbors ---*/
      
      for (iNode = 0; iNode < AuxQueue.size(); iNode++) {
        for (jNode = 0; jNode < AuxQueue.size() - 1 - iNode; jNode++) {
          if (node[AuxQueue[jNode]]->GetnPoint() > node[AuxQueue[jNode+1]]->GetnPoint()) {
            AuxPoint = AuxQueue[jNode];
            AuxQueue[jNode] = AuxQueue[jNode+1];
            AuxQueue[jNode+1] = AuxPoint;
          }
        }
      }
      
      Queue.insert(Queue.end(), AuxQueue.begin(), AuxQueue.end());
      for (iNode = 0; iNode < AuxQueue.size(); iNode++) {
        inQueue[AuxQueue[iNode]] = true;
      }
      
    }
    
    /*--- Extract the first node from the queue and add it in the first free
     position. ---*/
    
    if (Queue.size() != 0) {
      AddPoint = Queue[0];
      Result.push_back(Queue[0]);
      Queue.erase (Queue.begin(), Queue.begin()+1);
    }
    
    /*--- Add to the queue all the nodes adjacent in the increasing
     order of their degree, checking if the element is already
     in the Queue. ---*/
    
  } while (Queue.size() != 0);
  
  /*--- Check that all the points have been added ---*/
  
  for (iPoint = 0; iPoint < nPointDomain; iPoint++) {
    if (inQueue[iPoint] == false) Result.push_back(iPoint);
  }
  
  delete[] inQueue;
  
  reverse(Result.begin(), Result.end());
  
  /*--- Add the MPI points ---*/
  
  for (iPoint = nPointDomain; iPoint < nPoint; iPoint++) {
    Result.push_back(iPoint);
  }
  
  /*--- Reset old data structures ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    node[iPoint]->ResetElem();
    node[iPoint]->ResetPoint();
    node[iPoint]->ResetBoundary();
    node[iPoint]->SetPhysicalBoundary(false);
    node[iPoint]->SetSolidBoundary(false);
    node[iPoint]->SetDomain(true);
  }
  
  /*--- Set the new coordinates ---*/
  
  su2double **AuxCoord;
  unsigned long *AuxGlobalIndex;
  
  AuxGlobalIndex = new unsigned long [nPoint];
  AuxCoord = new su2double* [nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    AuxCoord[iPoint] = new su2double [nDim];
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    AuxGlobalIndex[iPoint] = node[iPoint]->GetGlobalIndex();
    for (iDim = 0; iDim < nDim; iDim++) {
      AuxCoord[iPoint][iDim] = node[iPoint]->GetCoord(iDim);
    }
  }
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    node[iPoint]->SetGlobalIndex(AuxGlobalIndex[Result[iPoint]]);
    for (iDim = 0; iDim < nDim; iDim++)
      node[iPoint]->SetCoord(iDim, AuxCoord[Result[iPoint]][iDim]);
  }
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    delete[] AuxCoord[iPoint];
  delete[] AuxCoord;
  delete[] AuxGlobalIndex;
  
  /*--- Set the new conectivities ---*/
  
  unsigned long *InvResult;
  InvResult = new unsigned long [nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    InvResult[Result[iPoint]] = iPoint;
  
  for (iElem = 0; iElem < nElem; iElem++) {
    for (iNode = 0; iNode < elem[iElem]->GetnNodes(); iNode++) {
      iPoint = elem[iElem]->GetNode(iNode);
      elem[iElem]->SetNode(iNode, InvResult[iPoint]);
    }
  }
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++) {
      
      string Marker_Tag = config->GetMarker_All_TagBound(iMarker);
      if (Marker_Tag == "SEND_RECEIVE") {
        for (unsigned long iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          if (config->GetMarker_All_SendRecv(iMarker) < 0)
            node[bound[iMarker][iElem_Bound]->GetNode(0)]->SetDomain(false);
        }
      }
      
      for (iNode = 0; iNode < bound[iMarker][iElem]->GetnNodes(); iNode++) {
        iPoint = bound[iMarker][iElem]->GetNode(iNode);
        bound[iMarker][iElem]->SetNode(iNode, InvResult[iPoint]);
        node[InvResult[iPoint]]->SetBoundary(nMarker);
        if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE &&
            config->GetMarker_All_KindBC(iMarker) != INTERNAL_BOUNDARY &&
            config->GetMarker_All_KindBC(iMarker) != INTERFACE_BOUNDARY &&
            config->GetMarker_All_KindBC(iMarker) != NEARFIELD_BOUNDARY &&
            config->GetMarker_All_KindBC(iMarker) != PERIODIC_BOUNDARY)
          node[InvResult[iPoint]]->SetPhysicalBoundary(true);
        
        if (config->GetMarker_All_KindBC(iMarker) == EULER_WALL ||
            config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX ||
            config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)
          node[InvResult[iPoint]]->SetSolidBoundary(true);
      }
    }
  }
  
  
  delete[] InvResult;
  
}

void CPhysicalGeometry::SetElement_Connectivity(void) {
  unsigned short first_elem_face, second_elem_face, iFace, iNode, jElem;
  unsigned long face_point, Test_Elem, iElem;
  
  /*--- Loop over all the elements, faces and nodes ---*/
  
  for (iElem = 0; iElem < nElem; iElem++)
    for (iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++)
      for (iNode = 0; iNode < elem[iElem]->GetnNodesFace(iFace); iNode++) {
        face_point = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace, iNode));
        
        /*--- Loop over all elements sharing the face point ---*/
        
        for (jElem = 0; jElem < node[face_point]->GetnElem(); jElem++) {
          Test_Elem = node[face_point]->GetElem(jElem);
          
          /*--- If it is a new element in this face ---*/
          
          if ((elem[iElem]->GetNeighbor_Elements(iFace) == -1) && (iElem < Test_Elem) &&
              (FindFace(iElem, Test_Elem, first_elem_face, second_elem_face))) {
            
            /*--- Localice which faces are sharing both elements ---*/
            
            elem[iElem]->SetNeighbor_Elements(Test_Elem, first_elem_face);
            
            /*--- Store the element for both elements ---*/
            
            elem[Test_Elem]->SetNeighbor_Elements(iElem, second_elem_face);
            
          }
        }
      }
}
int CPhysicalGeometry::SetWenoNumCells(void){
	//functions returns the array of elements for which a WENO stencil can be created.
	int elemCount = 0;
    unsigned long iElem, iPoint, jPoint, kPoint;
    int node_check = 0;
    int iNeigh = 0;
	for(iElem = 0; iElem < nElem; iElem++){
	//	iElem = 0;
		float elem_node_container[elem[iElem]->GetnNodes()];
		int check_boundary = 0;
		for(int i = 0; i < elem[iElem]->GetnNodes(); i++){
			elem_node_container[i] = elem[iElem]->GetNode(i);
			//checking if the nodes of the element lie on the boundary if they do then dont need to construct the stencil
			iPoint = elem_node_container[i];
			check_boundary = node[iPoint]->GetBoundary();
			if(check_boundary == 1){
			//	cout << "The element: " << iElem << " has a node on the boundary" << endl;
				node_check += 1;
				break;
			}else{
				//iPoint = elem_node_container[i];
				for(iNeigh = 0; iNeigh < node[iPoint]->GetnPoint(); iNeigh++){
					jPoint = node[iPoint]->GetPoint(iNeigh);
					check_boundary = node[jPoint]->GetBoundary();
					if(check_boundary == 1){
			//			cout << "The element: " << iElem << " has a neighbour with a node on the boundary" << endl;
						node_check += 1;
						break;
					}else{
						for(int j = 0; j < node[jPoint]->GetnPoint(); j++){
							kPoint = node[jPoint]->GetPoint(j);
							check_boundary = node[kPoint]->GetBoundary();
							if(check_boundary == 1){
								node_check += 1;
								break;
							}
						}
					}
				}
			}
		}
		if(node_check == 0){
			//stencil_container[elemCount] = iElem;		//contains the list of elements for which a stencil construction is possible
			elemCount += 1;
		}else{
			//cout << "Element: " << iElem << " has FAILED the requirements for stencil assembly" << endl;
		}
		node_check = 0;
	}
    return elemCount;
}

void CPhysicalGeometry::SetWeno_Stencil(void) {
	// Function to assemble the stencils for the WENO 3rd order scheme
	// cout << "Assembling the stencils for the WENO reconstruction" << endl;
	// variables initialisation
	// WENO 3 variables init (currently only for 2D)
	float Dc = 1.0; // value which dictates which precentage of number of nodes which lie in the sectorial plane for directional stencil assembly
	float elementDC = 0.0;
	int K, poly_order, M, L, num_neighbour = 0, num_neighbour2 = 0, boundary_cell = 0, inner_cell = 0, iNeigh, jNeigh, weno_index = 0, num_central = 0, nStencils;
	int noCentralStencil = 0, yesCentralStencil = 0;
	int noDirectionalStencil = 0, yesDirectionalStencil = 0;
	float fact_robust = 2.0; 
	su2double wall_distance;
	unsigned long neighbour, neighbour2, neighbour3, neighbour4,  neighbour_container1[6], neighbour_container2[6][6], iPoint, jPoint, kPoint;
//	bool check_boundary;
    unsigned short Node_Neighbor, iNode, iNeighbor;
    unsigned long jElem, Point_Neighbor, iElem;
    unsigned short first_elem_face, second_elem_face, iFace, iFace2;
    unsigned long face_point, Test_Elem, weno_element;
	long* directional_container;
	bool weno_check = 0;
	unsigned long testContainer[20]; 
	// Finding the neighbours of the neighbours
    ////looping over all elements in the domain
	// future functionality to add, poly_order should read the order of accuracy from config file and self initialise
    poly_order = 2;									// order of polynomial for reconstruction 
    K = 0.5*(poly_order + 1)*(poly_order + 2) - 1;	// minimum number of cells required in a stencil to achieve (poly_order + 1)th order accuracy
	M = fact_robust*K;								// to ensure robustness of scheme, multiply minimum number of cells required by factor
	// initialising the arrays containing the stencils, 0 is central stencil, 1 2 3 4 are for direction stencils, only need 4 if element is quadrilateral
	//size of stencil arrays scales with order of accuracy
	//int weno_cells = 6844;
    //int weno_cells = 16992; 
    int weno_cells = SetWenoNumCells();
	int first_order_cells = nElem - weno_cells; 
	unsigned long FO_container[first_order_cells]; 
	//cout << "weno_cells = " << weno_cells << endl;
    //cout << "nElem = " << nElem << endl;
    //cout << "First line of initialisation" << endl;
	unsigned long weno_elem_container0[M], weno_elem_container1[M+1], weno_elem_container2[M+1], weno_elem_container3[M+1], weno_elem_container4[M], main_weno_container[M][4], wenoPointContainer[3*M][4];
	//cout << "Second line of initialisation" << endl;
//    unsigned long main_weno_container2[nElem][4*M], main_central_container[nElem][M];
    //cout << "Third line of initialisation" << endl;
	//unsigned long centralContainer[nElem][M];
    //cout << "Fourth line of initialisation" << endl;
	//unsigned long wenoContainer[nElem][3*M];
    //cout << "Stencil Container matrix initialisation" << endl;
	int node_check = 0, elemCount = 0;
    int stencil_container[weno_cells];
	int foCount = 0;
	//unsigned long stencil_weno[8][4*M];
	unsigned long stencil_weno_central[M+1];
	
	stencil_weno = new unsigned long*[weno_cells];
	for(int i = 0; i < weno_cells; i++){
		stencil_weno[i] = new unsigned long[4*(M+1)];
		for(int j = 0; j < 4*(M+1); j++){
			stencil_weno[i][j] = 0.0; 
		}
	}
//    cout << "AFTER stencil weno initialisation" << endl;
	//unsigned long stencil_container[8];
//    cout << "Outputting the stencil_weno container" << endl;
 //   for(int i = 0; i < weno_cells; i++){
 //       for(int j = 0; j < 4*(M+1); j++){
  //          cout << stencil_weno[i][j] << " ";
  //      }
  //      cout << "\n";
  //  }
	// still need to set the element stencils some how, so that they can be accessed during the reconstruction stage.
	// need to make sure the sectorial stencils do not overlap!!!
	// looping over all the points of the domain pseudo central stencil

	for(int i = 0; i < M+1; i++){
	  weno_elem_container1[i] = 0.0; weno_elem_container2[i] = 0.0; weno_elem_container3[i] = 0.0;
	}
	//--------------------------------------------------------------------------------------------//
	//--------------------------------BOUNDARY CHECKER--------------------------------------------//
	//--------------------------------------------------------------------------------------------//
  //  cout << "Before the loop to fill the stencil container" << endl;
	for(iElem = 0; iElem < nElem; iElem++){
	//	iElem = 0;
		float elem_node_container[elem[iElem]->GetnNodes()];
		int check_boundary = 0;
		for(int i = 0; i < elem[iElem]->GetnNodes(); i++){
			elem_node_container[i] = elem[iElem]->GetNode(i);
			//checking if the nodes of the element lie on the boundary if they do then dont need to construct the stencil
			iPoint = elem_node_container[i];
			check_boundary = node[iPoint]->GetBoundary();
			if(check_boundary == 1){
			//	cout << "The element: " << iElem << " has a node on the boundary" << endl;
				node_check += 1;
				break;
			}else{
				//iPoint = elem_node_container[i];
				for(iNeigh = 0; iNeigh < node[iPoint]->GetnPoint(); iNeigh++){
					jPoint = node[iPoint]->GetPoint(iNeigh);
					check_boundary = node[jPoint]->GetBoundary();
					if(check_boundary == 1){
			//			cout << "The element: " << iElem << " has a neighbour with a node on the boundary" << endl;
						node_check += 1;
						break;
					}else{
						for(int j = 0; j < node[jPoint]->GetnPoint(); j++){
							kPoint = node[jPoint]->GetPoint(j);
							check_boundary = node[kPoint]->GetBoundary();
							if(check_boundary == 1){
								node_check += 1;
								break;
							}
						}
					}
				}
			}
		}
		if(node_check == 0){
			stencil_container[elemCount] = iElem;		//contains the list of elements for which a stencil construction is possible
			elemCount += 1;
		}else{
			FO_container[foCount] = iElem;				//contains the list of elements for which First order reconstruction is used.
			foCount += 1; 					
			//cout << "Element: " << iElem << " has FAILED the requirements for stencil assembly" << endl;
		}
		node_check = 0;
	}
    //cout << "Iterating over the elements to fill stencil container" << endl;
		//checking the neighbours of the cell to see if they have nodes which lie on the boundary
//	cout << "Outputting the cells for which WENO reconstruction is done" << endl;
//	for(int i = 0; i < elemCount; i++){
//		cout << stencil_container[i] << endl;
//	}
//	cout << "\n" << endl;
//	cout << "Outputting the cells for which First order reconstruction is done" << endl;
//	for(int i = 0; i < foCount; i++){
//		cout << FO_container[i] << endl;
//	}
//}
	
	//-------------------------------------------------------------------------------------------//
	//-----------------------------------WENO STENCIL ASSEMBLY-----------------------------------//
	//-------------------------------------------------------------------------------------------//
	//for(iElem = 0; iElem < nElem; iElem++){
    //cout << "Filling the central stencil" << endl;
	for(int i = 0; i < elemCount; i++){
		iElem = stencil_container[i];
	//-------------------------------------------------------------------------------------------//
	//----------------------------------CENTRAL STENCIL ASSEMBLY---------------------------------//
	//-------------------------------------------------------------------------------------------//
		stencil_weno_central[0] = iElem;
		num_neighbour2 += 1;
		for(iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++){
			stencil_weno_central[iFace+1] = elem[iElem]->GetNeighbor_Elements(iFace);
			num_neighbour2 += 1;
		}
	//	cout << "num_neighbour = " << num_neighbour << endl;
		weno_check = 0;
		int factor = 1;
		while(num_neighbour2 < (M+1)){
			weno_element = stencil_weno_central[factor];
		//	cout << "weno_element = " << weno_element << endl;
		//	cout << "just before for loop" << endl;
			for(iFace2 = 0; iFace2 < elem[weno_element]->GetnFaces(); iFace2++){
			//	cout << "inside for loop" << endl;
				neighbour = elem[weno_element]->GetNeighbor_Elements(iFace2);
			//	cout << "neighbour check" << endl;
				for(int j = 0; j < M; j++){
					if(neighbour == stencil_weno_central[j]){
	for(int i = 0; i < 20; i++){
		testContainer[i] = 0.0;
	}						weno_check = 1;
					}
				}
				if(weno_check == 0){
					stencil_weno_central[num_neighbour2] = neighbour;
					num_neighbour2 += 1;
				}
				weno_check = 0;
			}
			factor += 1;
		}
		num_neighbour2 = 0;
		for(int j = 0; j < M+1; j++){
			stencil_weno[i][j] = stencil_weno_central[j];
		}
//	    cout << "---------------------------------------------------------------" << endl;
//		cout << "-----------------PRINTING CENTRAL STENCIL----------------------" << endl;
//		cout << "---------------------------------------------------------------" << endl;
//		for(int i = 0; i < M+1; i++){
//			cout << stencil_weno_ce55 , 54 , 53 , 70 , 35 , 37 , 20 , 36 , 19 , 21 , 38ntral[i] << " , ";
//		}
	//-------------------------------------------------------------------------------------------//
	//-------------------------------DIRECTIONAL STENCIL ASSEMBLY--------------------------------//
	//-------------------------------------------------------------------------------------------// 
		
   }

 //  cout << "-----------END OF CENTRAL STENCIL ASSEMBLY-------------------" << endl;
 //  cout << "-----------BEGINNING DIRECTIONAL STENCIL ASSEMBLY------------" << endl;
 
   int testNeighbour = 0; 
   int testCheck = 0;
   unsigned long candidateNeighbour;
   unsigned long testElement;
   //K = 5; M = 2*K;
   int wenoIter = 1, wenoIter2 = 0, wenoIter3 = 0; 
   unsigned long testStencil_Central[M+1];
   unsigned long testStencil_Directional[M+1];
   for(int i = 0; i < (M+1); i++){
	   testStencil_Central[i] = 0;
	   testStencil_Directional[i] = 0;
   }
   //iElem = 70;
   for(int ii = 0; ii < elemCount; ii++){
	   iElem = stencil_container[ii];
	   testStencil_Central[0] = iElem;
	   testNeighbour += 1;
	 //  cout << "iElem = " << iElem << endl;
	   su2double MBC[2], candidateCells[2*(K+1)], CBC[2*(K+1)][2]; // MBC = master barycentre, difference between barycentre of candidate cell and master cell
	   su2double containerBC[2];
	   unsigned long masterNode1, masterNode2, masterNode3;
	   unsigned long testNode1, testNode2, testNode3, canCell;
	   su2double smallestDistance = 10000.0;
	   unsigned long smallestDistanceCell;
	   
	   //Master cell bary centre calculation
	   masterNode1 = elem[iElem]->GetNode(0); masterNode2 = elem[iElem]->GetNode(1); masterNode3 = elem[iElem]->GetNode(2);
	   MBC[0] = (1.0/3.0)*(node[masterNode1]->GetCoord(0) + node[masterNode2]->GetCoord(0) + node[masterNode3]->GetCoord(0));
	   MBC[1] = (1.0/3.0)*(node[masterNode1]->GetCoord(1) + node[masterNode2]->GetCoord(1) + node[masterNode3]->GetCoord(1));

	   while(testNeighbour < (M+1)){
		   for(iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++){
			   candidateNeighbour = elem[iElem]->GetNeighbor_Elements(iFace);
			   for(int i = 0; i < (M+1); i++){	//checking if the element already exists in the stencil.
				   if(testStencil_Central[i] == candidateNeighbour){
					   testCheck = 1;
				   }
			   }
				if(testCheck == 0){
				   testStencil_Central[testNeighbour] = candidateNeighbour;
				   testNeighbour += 1;				   
				}
			   testCheck = 0;
		   }
			iElem = testStencil_Central[wenoIter];
			wenoIter += 1;
			wenoIter2 = K-1;
			
			if(testNeighbour == M){
				while(wenoIter3 < (M-1)){
					testElement = testStencil_Central[wenoIter2];
					//cout << "Test Element = " << testElement << endl;
					//finding the last element in the stencil involves checking which of the face direct neighbouring cells are closest to the master cell
					for(int i = 0; i < elem[wenoIter2]->GetnFaces(); i++){
						testNeighbour = elem[testElement]->GetNeighbor_Elements(i);
						for(int j = 0; j < (M+1); j++){
							if(testStencil_Central[j] == testNeighbour){
								testCheck = 1;
							}
						}
						if(testCheck == 0){
							candidateCells[wenoIter3] = testNeighbour;
							wenoIter3 += 1;							
						}
						testCheck = 0;
					}
					wenoIter2 += 1;
				}
				//cout <<"Outputting a list of candidate cells" << endl;
				//for(int i = 0; i < 2*(K+1); i++){
				//	cout << candidateCells[i] << " , ";
				//}
				//cout << "\n" << endl;
				//created a list of candidate cells
				// now calculating barycentre and distance to main cell storing for each candidate cell
				for(int i = 0; i < 2*(K+1); i++){
					canCell = candidateCells[i];
					testNode1 = elem[canCell]->GetNode(0); testNode2 = elem[canCell]->GetNode(1); testNode3 = elem[canCell]->GetNode(2);
					containerBC[0] = (1.0/3.0)*(node[testNode1]->GetCoord(0) + node[testNode2]->GetCoord(0) + node[testNode3]->GetCoord(0));
					containerBC[1] = (1.0/3.0)*(node[testNode1]->GetCoord(1) + node[testNode2]->GetCoord(1) + node[testNode3]->GetCoord(1));
					CBC[i][0] = canCell;		//storing cell ID
					CBC[i][1] = sqrt((containerBC[0] - MBC[0])*(containerBC[0] - MBC[0]) + (containerBC[1] - MBC[1])*(containerBC[1] - MBC[1]));//calculating barycentre distance
					if(CBC[i][1] < smallestDistance){
						smallestDistance = CBC[i][1];
						smallestDistanceCell = CBC[i][0];
						//testStencil_Central[M] = CBC[i][0];
					}
					//cout << CBC[i][0] << " , " << CBC[i][1] << endl;
				}
				//cout << "Smallest Distance Cell = " << smallestDistanceCell << endl;
				testStencil_Central[M] = smallestDistanceCell;
				testNeighbour += 1;
			}
			//testStencil_Central[M] = smallestDistanceCell;
	   }
	   testNeighbour = 0; testCheck = 0; wenoIter = 1; wenoIter2 = 0; wenoIter3 = 0;
//	   cout << "Central Stencil NEW" << endl;
//		for(int j = 0; j < (M+1); j++){
//			//stencil_weno[i][j] = testStencil_Central[j];
//			cout << testStencil_Central[j] << " , ";
//			testStencil_Central[j] = 0;
//		}
//		cout << "\n" << endl;
//		cout << "Central Stencil OLD" << endl;
//		for(int j = 0; j < (M+1); j++){
//			cout << stencil_weno[ii][j] << " , ";
//		}
//		cout << "\n" << endl;
		//exit(0);
		for(int j = 0; j < (M+1); j++){
			stencil_weno[ii][j] = testStencil_Central[j];
		}
	}

 // for (iElem = 0; iElem < nElem; iElem++){
    // cout << "Filling the directional stencil" << endl;
    for(int i = 0; i < elemCount; i++){
		iElem = stencil_container[i];
		//cout << "Stencil Assembly for Element = " << stencil_container[i] << endl;
		  for(iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++){
			  // for each face of the element initialise the appropriate directional stencil container
			  if(iFace == 0){
				  weno_elem_container1[0] = iElem;
				  weno_elem_container1[1] = elem[iElem]->GetNeighbor_Elements(iFace);
				//weno_elem_container1[0] = elem[iElem]->GetNeighbor_Elements(iFace);
				  //cout << "Element " << iElem << " Face = " << iFace << " Face Neighbour = " << weno_elem_container1[1] << endl;
			  }else if(iFace == 1){
				  weno_elem_container2[0] = iElem;
				  weno_elem_container2[1] = elem[iElem]->GetNeighbor_Elements(iFace);
				//weno_elem_container2[0] = elem[iElem]->GetNeighbor_Elements(iFace);
				  //cout << "Element " << iElem << " Face = " << iFace << " Face Neighbour = " << weno_elem_container2[1] << endl;
			  }else{
				  weno_elem_container3[0] = iElem;
				  weno_elem_container3[1] = elem[iElem]->GetNeighbor_Elements(iFace);
				//weno_elem_container3[0] = elem[iElem]->GetNeighbor_Elements(iFace);
				  //cout << "Element " << iElem << " Face = " << iFace << " Face Neighbour = " << weno_elem_container3[1] << endl;
			  }
			  num_neighbour += 2;		//Pre-fix 07/08/2017
			  weno_index = 1;			//Pre-fix 07/08/2017
			  //num_neighbour += 1;
			  //weno_index = 0;
		  //while loop which calls the directional stencil assembly and exit criterion 
			  //cout << "Number of elements required in stencil = " << M << endl;
			  while(num_neighbour < (M+1)){
				  if(iFace == 0){
					weno_element = weno_elem_container1[weno_index];
				  }else if(iFace == 1){
					weno_element = weno_elem_container2[weno_index];
				  }else{
					weno_element = weno_elem_container3[weno_index];
				  }
				  //cout << "Weno index = " << weno_index << " Weno Element = " << weno_element << endl; 

				  //cout << "Number of accepted neighbours = " << num_neighbour << endl;
				  for(iFace2 = 0; iFace2 < elem[weno_element]->GetnFaces(); iFace2++){
					  neighbour = elem[weno_element]->GetNeighbor_Elements(iFace2);
					  for(int i = 0; i < M; i++){
						  //cout << neighbour << " , " << weno_elem_container1[i] << endl;
						  if(iFace == 0){
							  if(neighbour == weno_elem_container1[i]){
								  weno_check = 1;
							  }
						  }else if(iFace == 1){
							  if(neighbour == weno_elem_container2[i]){
								  weno_check = 1;
							  }
						  }else{
							  if(neighbour == weno_elem_container3[i]){
								  weno_check = 1;
							  }
						  }
					  }
					  if(iFace == 0){
					   if(weno_check == 0){
							weno_elem_container1[num_neighbour] = neighbour;
							num_neighbour += 1;
					   }
					  }else if(iFace == 1){
						  if(weno_check == 0){
							  weno_elem_container2[num_neighbour] = neighbour;
							  num_neighbour += 1;
						  }
					  }else{
						  if(weno_check == 0){
								  weno_elem_container3[num_neighbour] = neighbour;
							  num_neighbour += 1;
						  }
					  }
					  weno_check = 0; 
				  }
				  weno_index += 1;
				}
				num_neighbour = 0;		// reset neighbouir counter to 2 since on the next iteration we add the master element and its face neighbour to the stencil array anyways
				weno_index = 0;			// reset to one for same reason as above
				
			}

			for(int j = 0; j < M+1; j++){
				stencil_weno[i][j+M+1] = weno_elem_container1[j];
				stencil_weno[i][j+2*(M+1)] = weno_elem_container2[j];
				stencil_weno[i][j+3*(M+1)] = weno_elem_container3[j];
			}
	}
	
	//--------------------------------------------------------------------------------------//
	//----------------DIRECTIONAL STENCIL ASSEMBLY WITH BOUNDARIES--------------------------//
	//--------------------------------------------------------------------------------------//
	/*
	unsigned long testStencil_Directional1[M+1], testStencil_Directional2[M+1], testStencil_Directional3[M+1];
	su2double MBC[2], candidateCells[2*(K+1)], CBC[2*(K+1)][2]; // MBC = master barycentre, difference between barycentre of candidate cell and master cell
	su2double containerBC[2];
	unsigned long masterNode1, masterNode2, masterNode3;
	unsigned long testNode1, testNode2, testNode3, canCell;
	su2double smallestDistance = 10000.0;
	unsigned long smallestDistanceCell;
  
   //Master cell bary centre calculation
    masterNode1 = elem[iElem]->GetNode(0); masterNode2 = elem[iElem]->GetNode(1); masterNode3 = elem[iElem]->GetNode(2);
    MBC[0] = (1.0/3.0)*(node[masterNode1]->GetCoord(0) + node[masterNode2]->GetCoord(0) + node[masterNode3]->GetCoord(0));
    MBC[1] = (1.0/3.0)*(node[masterNode1]->GetCoord(1) + node[masterNode2]->GetCoord(1) + node[masterNode3]->GetCoord(1));	
	int numDirectional1 = 0;
	for(int i = 0; i < elemCount; i++){
		iElem = stencil_container[i];
		// adding the master element to index 0 of the directional stencil
		testStencil_Directional1[0] = iElem;
		//adding the first face sharing neighbour to the stencil
		testStencil_Directional1[1] = elem[iElem]->GetNeighbor_Elements(0);
		numDirectional1 = 2;
		//obtaining the nodes of the face 
		unsigned long face1, faceNode1, faceNode2;
		unsigned long xfaceNode1, yfaceNode1, xfaceNode2, yfaceNode2;  
		su2double gradFace1, gradFace2;
		iFace = 0;
		faceNode1 = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,0));
		faceNode2 = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,1));
		xfaceNode1 = node[faceNode1]->GetCoord(0); yfaceNode1 = node[faceNode1]->GetCoord(1);
		xfaceNode2 = node[faceNode2]->GetCoord(0); yfaceNode2 = node[faceNode2]->GetCoord(1);
		
		//calculating the gradient of the line joining the First node of the face and the barycentre of the master cell
		// added the 1E-06 to prevent division by 0
		gradFace1 = (xfaceNode1 - MBC[1])/(xfaceNode1 - MBC[0] + 1E-06);
		//calculating the gradient of the line joining the Second node of the face and the barycentre of the master cell
		gradFace2 = (yfaceNode2 - MBC[1])/(xfaceNode2 - MBC[0] + 1E-06);
		// calculating the line constant using y - b = m(x - a) rearranging into the form y = mx + c where c = -am + b
		su2double lineConstant1, lineConstant2;
		lineConstant1 = -xfaceNode1*gradFace1 + yfaceNode1;
		lineConstant2 = -xfaceNode2*gradFace2 + yfaceNode2;
		su2double DcTarget = 0.99, canDc;
		// adding further elements to the directional stencil
		int iterate = 1, directional1_check1 = 0, directional1_check2 = 0;
		unsigned long canNode1, canNode2, canNode3;
		unsigned long xcanNode1, ycanNode1, xcanNode2, ycanNode2, xcanNode3, ycanNode3; 
		su2double xAlpha, xBeta, yAlpha, yBeta, xUpper, xLower, yUpper, yLower;
		int numNodes = 0;
		while(numDirectional1 < (M+1)){
			for(iFace = 0; iFace < elem[testStencil_Directional1[iterate]]->GetnFaces(); iFace++){
				unsigned long neighbourD1;
				neighbourD1 = elem[testStencil_Directional1[iterate]]->GetNeighbor_Elements(iFace); 
				cout << "NeighbourD1 = " << neighbourD1 << endl;
				// Repetition Condition Check
				for(int j = 0; j < (M+1); j++){
					cout << neighbourD1 << " , " << testStencil_Directional1[j] << endl;
					if(neighbourD1 == testStencil_Directional1[j]){
						directional1_check1 = 1;
					}
				}
				// Geometrical Condition Check
				// if passes the repetition condition check, do the geometrical check
				if(directional1_check1 == 0){
					// obtaining the nodes of the cell
					canNode1 = elem[neighbourD1]->GetNode(0); canNode2 = elem[neighbourD1]->GetNode(1); canNode3 = elem[neighbourD1]->GetNode(2);
					xcanNode1 = node[canNode1]->GetCoord(0); ycanNode1 = node[canNode1]->GetCoord(1);
					xcanNode2 = node[canNode2]->GetCoord(0); ycanNode2 = node[canNode2]->GetCoord(1);
					xcanNode3 = node[canNode3]->GetCoord(0); ycanNode3 = node[canNode3]->GetCoord(1);
					// checking to see if the nodes lie within the zone of the cell
					// Node 1
					yAlpha = gradFace1*xcanNode1 + lineConstant1; 
					yBeta = gradFace2*xcanNode1 + lineConstant2;
					if(yAlpha > yBeta){
						yLower = yBeta; yUpper = yAlpha;
					}else{
						yLower = yAlpha; yUpper = yBeta;
					}
					xAlpha = (ycanNode1 - lineConstant1)/gradFace1; 
					xBeta = (ycanNode1 - lineConstant2)/gradFace2;
					if(xAlpha > xBeta){
						xLower = xBeta; xUpper = xAlpha;
					}else{
						xLower = xAlpha; xUpper = xBeta;
					}
					if(xcanNode1 >= xLower && xcanNode1 <= xUpper && ycanNode1 >= yLower && ycanNode1 && yUpper <= ycanNode1){
						numNodes += 1;
					}
					// Node 2
					yAlpha = gradFace1*xcanNode2 + lineConstant1; 
					yBeta = gradFace2*xcanNode2 + lineConstant2;
					if(yAlpha > yBeta){
						yLower = yBeta; yUpper = yAlpha;
					}else{
						yLower = yAlpha; yUpper = yBeta;
					}
					xAlpha = (ycanNode2 - lineConstant1)/gradFace1; 
					xBeta = (ycanNode2 - lineConstant2)/gradFace2;
					if(xAlpha > xBeta){
						xLower = xBeta; xUpper = xAlpha;
					}else{
						xLower = xAlpha; xUpper = xBeta;
					}
					if(xcanNode2 >= xLower && xcanNode2 <= xUpper && ycanNode2 >= yLower && ycanNode2 && yUpper <= ycanNode2){
						numNodes += 1;
					}
					// Node 3
					yAlpha = gradFace1*xcanNode3 + lineConstant1; 
					yBeta = gradFace2*xcanNode3 + lineConstant2;
					if(yAlpha > yBeta){
						yLower = yBeta; yUpper = yAlpha;
					}else{
						yLower = yAlpha; yUpper = yBeta;
					}
					xAlpha = (ycanNode3 - lineConstant1)/gradFace1; 
					xBeta = (ycanNode3 - lineConstant2)/gradFace2;
					if(xAlpha > xBeta){
						xLower = xBeta; xUpper = xAlpha;
					}else{
						xLower = xAlpha; xUpper = xBeta;
					}
					if(xcanNode3 >= xLower && xcanNode3 <= xUpper && ycanNode3 >= yLower && ycanNode3 && yUpper <= ycanNode3){
						numNodes += 1;
					}
					canDc = numNodes/elem[neighbourD1]->GetnNodes();
					if(canDc <= Dc){
						cout << "The element " << neighbourD1 << " has been selected as part of the stencil." << endl;
						testStencil_Directional1[numDirectional1] = neighbourD1;
						numDirectional1 += 1;
					}
				}
				directional1_check1 = 0;
				iterate += 1;
			}
		}
		exit(0);
	}
	*/
}

void CPhysicalGeometry::SetWenoEdgeElement(void){
	// be sure to call this method after the edges have been defined in the driver_structure.cpp
	unsigned long numEdges = GetnEdge();
	unsigned long iPoint, jPoint; 
	int elementCheck = 0, element1Iter = 0, element2Iter = 0;
	unsigned long element1 = 0, element2 = 0;
	//int weno_cells = 6844;
    int weno_cells = SetWenoNumCells();
    unsigned long*** nodalContainer = GetWeno_NodalContainer();
    /*
    stencil_weno = (unsigned long**)malloc(weno_cells*sizeof(unsigned long*));
    for(int i = 0; i < weno_cells; i++){
        stencil_weno[i] = (unsigned long*)malloc((4*(M+1))*sizeof(unsigned long));  
        for(int j = 0; j < 4*(M+1); j++){
            stencil_weno[i][j] = 0;
        }
    }
    for(int i = 0; i < weno_cells; i++){
        //stencil_weno[i] = new unsigned long[4*(M+1];
        stencil_weno[i] = (unsigned long*)malloc(4*(M+1)*sizeof(unsigned long));
        for(int j = 0; j < 4*(M+1); j++){
            stencil_weno[i][j] = 0; 
        }
    }
    */
	//edgeElement = (unsigned long**)malloc(numEdges*sizeof(unsigned long*));
    //for(int i = 0; i < numEdges; i++){
    //    edgeElement[i] = (unsigned long*)malloc(4*sizeof(unsigned long));
    //    edgeElement[i][0] = 0; edgeElement[i][1] = 0; edgeElement[i][2] = 0; edgeElement[i][3] = 0;
   // }
    // OLD INIT
	edgeElement = new unsigned long*[numEdges];
	for(int i = 0; i < numEdges; i++){
		edgeElement[i] = new unsigned long[4];
		edgeElement[i][0] = 0; edgeElement[i][1] = 0; edgeElement[i][2] = 0; edgeElement[i][3] = 0;
	}
	
	for(int iEdge = 0; iEdge < numEdges; iEdge++){ 
		//iterating over the edges of the primal mesh in the domain.
		iPoint = edge[iEdge]->GetNode(0); jPoint = edge[iEdge]->GetNode(1);
		for(int i = 0; i < weno_cells; i++){	// iterating over all the master elements which have stencils, i < 8
				for(int j = 0; j < 3; j++){		// iterating over the 3 nodes of a triangle
					if(iPoint == nodalContainer[i][0][j]){
					for(int k = 0; k < 3; k++){
						if(jPoint == nodalContainer[i][0][k]){
							if(elementCheck == 0){
 								element1 = stencil_weno[i][0];	//obtaining one of two elements which share the common edge between iPoint and jPoint
								element1Iter = i;
								elementCheck = 1;
							}else{
								element2 = stencil_weno[i][0];	// obtaining one of two elements which share the common edge between iPoint and jPoint
								element2Iter = i;
								//elementCheck = 0;
							}
						}
					}
				}
			}
		}
		edgeElement[iEdge][0] = element1; edgeElement[iEdge][1] = element1Iter; edgeElement[iEdge][2] = element2; edgeElement[iEdge][3] = element2Iter; 
		elementCheck = 0; element1 = 0; element2 = 0; element1Iter = 0; element2Iter = 0;
	}
	//cout << "Outputting the WENO Elements in WENO Edge element" << endl;
	
	//for(int i = 0; i < numEdges; i++){
	//	cout << "iEdge = " << i << " Edge Element 1 = " << edgeElement[i][0] << " Edge Element 2 =  " << edgeElement[i][2] << endl;
	//	cout << "element1Iter = " << edgeElement[i][1] << " , element2Iter = " << edgeElement[i][3] << endl;
	//	cout << "\n";
	//	cout << "iEdge = " << i << " iPoint = " << edge[i]->GetNode(0) << " jPoint = " << edge[i]->GetNode(1) << endl;
 	//}
	//exit(0);
}

su2double CPhysicalGeometry::SetWenoBasisFunction(int iter, su2double x, su2double y){
	// this function returns Phi_k based on an integer in and the vertex points of target cell element in referece space
	// which will access the appropriate phi_k(x1, y1) = phi_k(epsilon, eta) from a predetermined psi_k currently chosen to be the standard orthogonal basis functions
	// x1 is epsilon in reference space
	// y1 is eta in reference space
	// variable initialisation
	
	// 14/02/2018 
	// basis_rec is now phi_k = psi_k - 1/cell_area * integral of psi_k over cell 
	// where psi_k = psi, eta, psi**2 etc.... depending on polynomial desired
	// for loop assigning and storing the appropriate phi_k 
	// defining the orthogonal basis functions in reference space 
	// potential mistake here, in the sense that might need to make the integration with generic
	// by this I mean, the master node has a mapping which maps the physical triangle to a unit triangle in reference space
	// the neighoburing cells are then mapped from physical to reference space using the mapping optained from the master triangle
	// which will affect the limits of the integral of the basis function over that triangular element
	// the current implementation evaluates the integral of the basis function of the unit triangle
	// although since every triangle is mapped to a unit triangle in reference space then the orientation of the triangle should be irrelevant ?
	// since the function is still varying over a unit triangle. (never mind, stays the same)
	// the following are a set of generic orthogonal basis functions used in reference space
	// according to the AZURE (UCNS3D) code we can use Shifted Legendre Polynomials as well but is more expensive.
	// might be worth investigating the use of different polynomials, like Legendre or Shifted legendre.
	// for all cases the S corresponds to the Psi_k term from the literature
	// for all cases the basis_rec corresponds to Phi_k from the literature and euqals Psi_{k} - (1/reference_cell_area)*(integral of Psi_{k} over reference_cell)
	// ASSUMPTION, since all the functions are evaluated over the master node, where the master node is transformerd
	// from physical to reference space (unit triangle), the analytical expression are input, might have to add a generalised
	// gauss quadrature integration procedure to handle this integration
	// x1 = epsilon, y1 = eta (2D) functions only so far	
	
	su2double S, basis_rec; 
	bool GEN_ORTHOG = true; 
	bool SHIFTED_LEGENDRE = false;
	bool REGULAR_LEGENDRE = false;
	
	if(GEN_ORTHOG){
	// Using generic orthogonal polynomials as basis functions
		if(iter == 1){
			basis_rec = x - 1.0/3.0;
		}else if(iter == 2){
			basis_rec = y - 1.0/3.0;
		}else if(iter == 3){
			basis_rec = x*x - 1.0/6.0;
		}else if(iter == 4){
			basis_rec = x*y - 1.0/12.0;
		}else if(iter == 5){
			basis_rec = y*y - 1.0/6.0;
		}else if(iter == 6){
			basis_rec = x*x*x - 1.0/10.0;
		}else if(iter == 7){
			basis_rec = x*x*y - 1.0/30.0;
		}else if(iter == 8){
			basis_rec = y*y*x - 1.0/30.0;
		}else if(iter == 9){
			basis_rec = y*y*y - 1.0/10.0;
		}else if(iter == 10){
			basis_rec = x*x*x*x - 1.0/15.0;
		}else if(iter == 11){
			basis_rec = x*x*x*y - 1.0/60.0;
		}else if(iter == 12){
			basis_rec = x*x*y*y - 1.0/90.0;
		}else if(iter == 13){
			basis_rec = x*y*y*y - 1.0/60.0;
		}else if(iter == 14){
			basis_rec = y*y*y*y - 1.0/15.0;
		}
	}
	
	if(SHIFTED_LEGENDRE){
	// Using the Shifted Legendre Functions as basis functions
		if(iter == 1){
			basis_rec = 2.0*x - 7.0/6.0;
		}else if(iter == 2){
			basis_rec = 2.0*y - 7.0/6.0;
		}else if(iter == 3){
			basis_rec = 6.0*x*x - 6.0*x + 1.0;
		}else if(iter == 4){
			basis_rec = (2.0*x - 1.0)*(2.0*y - 1.0);
		}else if(iter == 5){
			basis_rec = 6.0*y*y - 6.0*y + 1.0;
		}else if(iter == 6){
			basis_rec = 20.0*x*x*x - 30.0*x*x + 12.0*x - 1.0;
		}else if(iter == 7){
			basis_rec = (6.0*x*x - 6.0*x + 1.0)*(2.0*y - 1.0) - 1.0/15.0;
		}else if(iter == 8){
			basis_rec = (6.0*y*y - 6.0*y + 1.0)*(2.0*x - 1.0) - 1.0/15.0;
		}else if(iter == 9){
			basis_rec = 20.0*y*y*y - 30.0*y*y + 12.0*y - 1.0;
		}else if(iter == 10){
			basis_rec = 70.0*x*x*x*x - 140.0*x*x*x + 90.0*x*x - 20.0*x + 1.0;
		}else if(iter == 11){
			basis_rec = (20.0*x*x*x - 30.0*x*x + 12.0*x - 1.0)*(2.0*y - 1.0);
		}else if(iter == 12){
			basis_rec = (6.0*x*x - 6.0*x + 1.0)*(6.0*y*y - 6.0*y + 1.0);
		}else if(iter == 13){
			basis_rec = (20.0*y*y*y - 30.0*y*y + 12.0*y - 1.0)*(2.0*x - 1.0);
		}else if(iter == 14){
			basis_rec = 70.0*y*y*y*y - 140.0*y*y*y + 90.0*y*y - 20.0*y + 1.0;
		}		
	}
	
	if(REGULAR_LEGENDRE){
	// Using Regular Legendre Functions as basis Functons
		if(iter == 1){
			basis_rec = x - 1.0/3.0;
		}else if(iter == 2){
			basis_rec = y - 1.0/3.0;
		}else if(iter == 3){
			basis_rec = 0.5*(3.0*x*x - 1.0) + 1.0/4.0;
		}else if(iter == 4){
			basis_rec = x*y - 1.0/12.0;
		}else if(iter == 5){
			basis_rec = 0.5*(3.0*y*y - 1.0) + 1.0/4.0;
		}else if(iter == 6){
			basis_rec = 0.5*(5.0*x*x*x - 3.0*x) - 3.0/4.0;
		}else if(iter == 7){
			basis_rec = 0.5*(3.0*x*x - 1.0)*y + 7.0/60.0;
		}else if(iter == 8){
			basis_rec = 0.5*(3.0*y*y - 1.0)*x + 7.0/60.0;
		}else if(iter == 9){
			basis_rec = 0.5*(5.0*y*y*y - 3.0*y) - 3.0/4.0;
		}else if(iter == 10){
			basis_rec = (1.0/8.0)*(35.0*x*x*x*x - 30.0*x*x + 3.0) - 1.0/24.0;
		}else if(iter == 11){
			basis_rec = 0.5*(5.0*x*x*x - 3.0*x)*y + 1.0/12.0;
		}else if(iter == 12){
			basis_rec = 0.25*(3.0*x*x - 1.0)*(3.0*y*y - 1.0) - 1.0/40.0;
		}else if(iter == 13){
			basis_rec = 0.5*(5.0*y*y*y - 3.0*y)*x + 1.0/12.0;
		}else if(iter == 14){
			basis_rec = (1.0/8.0)*(35.0*y*y*y*y - 30.0*y*y + 3.0) - 1.0/24.0;
		}		
	}
	/*
	// SECOND ORDER 
	if(iter == 1){
	//	basis_rec = x - 1.0/3.0;						//phi_k Generic Orthogonal Polynomial
		basis_rec = 2.0*x - 7.0/6.0;					//phi_k Shifted Legendre
	//	basis_rec = x - 1.0/3.0;						//phi_k Regular Legendre
	}else if(iter == 2){
	//	basis_rec = y - 1.0/3.0;						//phi_k based on generic orthogonal polynomials
		basis_rec = 2.0*y - 7.0/6.0;					//phi_k based on shifted legendre polynomials
	//  basis_rec = y - 1.0/3.0;						//phi_k based on regular Legendre Polynomials
	}else if(iter == 3){
	//THIRD ORDER	
	//	basis_rec = x*x - 1.0/6.0;						//phi_k based on generic orthogonal polynomials
		basis_rec = 6.0*x*x - 6.0*x + 1.0;				//phi_k based on shifted legendre polynomials
	//	basis_rec = 0.5*(3.0*x*x - 1.0) + 1.0/4.0;		//phi_k based on regular Legendre Polynomials
	}else if(iter == 4){
	//	basis_rec = x*y - 1.0/12.0;						//phi_k based on generic orthogonal polynomials
		basis_rec = (2.0*x - 1.0)*(2.0*y - 1.0);		//phi_k based on shifted legendre polynomials
	//	basis_rec = x*y - 1.0/12.0;						//phi_k based on regular legendre polynomials
	}else if(iter == 5){
	//	basis_rec = y*y - 1.0/6.0;						//phi_k based on generic orthogonal polynomials
		basis_rec = 6.0*y*y - 6.0*y + 1.0;				//phi_k based on shifted legendre polynomials
	//	basis_rec = 0.5*(3.0*x*x - 1.0) + 1.0/4.0;		//phi_k based on regular legendre polynomials
	}else if(iter == 6){
	//FOURTH ORDER
		basis_rec = x*x*x - 1.0/10.0;						//phi_k based on generic orthogonal polynomials
	//	basis_rec = 20.0*x*x*x - 30.0*x*x + 12.0*x - 1.0;	//phi_k based on shifted legendre polynomials
	//	basis_rec = ;
	}else if(iter == 7){
	//epsilon^2*eta
		S = x*x*y;
		basis_rec = x*x*y - 1.0/30.0;
	}else if(iter == 8){
		//epsilon*eta^2
		S = x*y*y;
		basis_rec = S - 1.0/30.0;
	}else if(iter == 9){
		//eta^3
		S = y*y*y;
		basis_rec = S - 1.0/10.0;
	}else if(iter == 10){
	//FIFTH ORDER 
		//epsilon^4
		S = x*x*x*x;
		basis_rec = x*x*x*x - 1.0/15.0;
	}else if(iter == 11){
		//epsilon^3*eta
		S = x*x*x*y;
		basis_rec = S - 1.0/60.0;	
	}else if(iter == 12){
		//epsilon^2*eta^2
		S = x*x*y*y;
		basis_rec = S - 1.0/90.0;
	}else if(iter == 13){
		//epsilon*eta^3
		S = x*y*y*y;
		basis_rec = S - 1.0/60.0;
	}else if(iter == 14){
		//eta^4
		S = y*y*y*y;
		basis_rec = S - 1.0/15.0;
	}
	*/
	return basis_rec;		//basis_rec = phi_k in the theory 
}

su2double* CPhysicalGeometry::WenoBasisFunction(int K){
	//returns an array where the value of phi_k is evaluated at iPoint and jPoint
	su2double basis_rec; 
	int iter = 0;
	su2double* phiK = new su2double[K];
	for(int i = 0; i < K; i++){
			iter = i + 1;
			if(iter == 1){
				//epsilon
				basis_rec = -1.0/3.0;
				//basis_rec = -1.0;
				//basis_rec = 0.0;	//evalated at x = 0.5, y = 0.5; 
				//basis_rec = -2.0/3.0;
			}else if(iter == 2){
				//eta
				basis_rec = -1.0/3.0;
				//basis_rec = -1.0;
				//basis_rec = 0.0;
				//basis_rec = -2.0/3.0;
			}else if(iter == 3){
			//THIRD ORDER
				//epsilon^2
				basis_rec = -1.0/6.0;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 4){
				//epsilon*eta
				basis_rec = -1.0/12.0;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 5){
				//eta^2
				basis_rec = -1.0/6.0;
				//basis_rec = 1.0;
				//basis_rec = 0.0; 
			}else if(iter == 6){
			//FOURTH ORDER
				//epsilon^3
				basis_rec = -1.0/10.0;
			}else if(iter == 7){
				//epsilon^2*eta
				basis_rec = -1.0/30.0;
			}else if(iter == 8){
				//epsilon*eta^2
				basis_rec = -1.0/30.0;
			}else if(iter == 9){
				//eta^3
				basis_rec = -1.0/10.0;
			}else if(iter == 10){
			//FIFTH ORDER 
				//epsilon^4
				basis_rec = -1.0/15.0;
			}else if(iter == 11){
				//epsilon^3*eta
				basis_rec = -1.0/60.0;	
			}else if(iter == 12){
				//epsilon^2*eta^2
				basis_rec = -1.0/90.0;
			}else if(iter == 13){
				//epsilon*eta^3
				basis_rec = -1.0/60.0;
			}else if(iter == 14){
				//eta^4
				basis_rec = -1.0/15.0;
			}else{
				basis_rec = 1.0;
			}
		phiK[i] = basis_rec;
	}
	return phiK;		
}

su2double* CPhysicalGeometry::WenoBasisFunctionAtPoint(int K, su2double x, su2double y){
su2double basis_rec; 
	int iter = 0;
	su2double* phiK = new su2double[K];
	for(int i = 0; i < K; i++){
			iter = i + 1;
			if(iter == 1){
				//epsilon
				basis_rec = x;
				//basis_rec = -1.0;
				//basis_rec = 0.0;	//evalated at x = 0.5, y = 0.5; 
				//basis_rec = -2.0/3.0;
			}else if(iter == 2){
				//eta
				basis_rec = y;
				//basis_rec = -1.0;
				//basis_rec = 0.0;
				//basis_rec = -2.0/3.0;
			}else if(iter == 3){
			//THIRD ORDER
				//epsilon^2
				basis_rec = x*x;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 4){
				//epsilon*eta
				basis_rec = x*y;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 5){
				//eta^2
				basis_rec = y*y;
				//basis_rec = 1.0;
				//basis_rec = 0.0; 
			}else if(iter == 6){
			//FOURTH ORDER
				//epsilon^3
				basis_rec = x*x*x;
				//basis_rec = -1.0/10.0;
			}else if(iter == 7){
				//epsilon^2*eta
				basis_rec = x*x*y;
				//basis_rec = -1.0/30.0;
			}else if(iter == 8){
				//epsilon*eta^2
				basis_rec = x*y*y;
				//basis_rec = -1.0/30.0;
			}else if(iter == 9){
				//eta^3
				basis_rec = y*y*y;
				//basis_rec = -1.0/10.0;
			}else if(iter == 10){
			//FIFTH ORDER 
				//epsilon^4
				basis_rec = x*x*x*x;
				//basis_rec = -1.0/15.0;
			}else if(iter == 11){
				//epsilon^3*eta
				basis_rec = x*x*x*y;
				//basis_rec = -1.0/60.0;	
			}else if(iter == 12){
				//epsilon^2*eta^2
				basis_rec = x*x*y*y;
				//basis_rec = -1.0/90.0;
			}else if(iter == 13){
				//epsilon*eta^3
				basis_rec = x*y*y*y;
				//basis_rec = -1.0/60.0;
			}else if(iter == 14){
				//eta^4
				basis_rec = y*y*y*y;
				//basis_rec = -1.0/15.0;
			}
		phiK[i] = basis_rec;
	}
	return phiK;		
}

su2double* CPhysicalGeometry::WenoBasisFunctionAtPointL(int K, unsigned long element1, unsigned long iPoint, unsigned long jPoint){
	su2double basis_rec; 
	su2double iPointx, iPointy, jPointx, jPointy; 
	su2double iPointXi, iPointEta, jPointXi, jPointEta;
	su2double deltax, deltay, cellFacex, cellFacey;											//position of the cell face in reference space
	su2double x,y;
//	unsigned long iPoint, jPoint; 
//	iPoint = edge[iEdge]->GetNode(0); jPoint = edge[iEdge]->GetNode(1);
	iPointx = node[iPoint]->GetCoord(0); iPointy = node[iPoint]->GetCoord(1);
	jPointx = node[jPoint]->GetCoord(0); jPointy = node[jPoint]->GetCoord(1);
	deltax = (iPointx - jPointx)/2.0; deltay = (iPointy - jPointy)/2.0; //finding the cell face distance relative to iPoint (x,y)
//	cout << "DeltaX = " << deltax << " Delta Y = " << deltay << endl;
	cellFacex = iPointx + deltax; cellFacey = iPointy + deltay;			//finding the position of cell Face in physical space
	int iter = 0;
	unsigned long node1, node2, node3; 
	node1 = elem[element1]->GetNode(0); node2 = elem[element1]->GetNode(1); node3 = elem[element1]->GetNode(2);
	su2double node1x, node1y, node2x, node2y, node3x, node3y;
	node1x = node[node1]->GetCoord(0); node1y = node[node1]->GetCoord(1);
	node2x = node[node2]->GetCoord(0); node2y = node[node2]->GetCoord(1);
	node3x = node[node3]->GetCoord(0); node3y = node[node3]->GetCoord(1);
	bool GEN_ORTHOG = true;
	bool SHIFTED_LEGENDRE = false; 
	bool REGULAR_LEGENDRE = false; 
	//commented out on 17/01/2018
	/*
	int node1Count = 0, node2Count = 0, node3Count = 0;
	if(node1x == node2x && node1y == node3y){
		node1Count += 1;
	}else if(node1x == node3x && node1y == node2y){
		node1Count += 1;
	}
	if(node2x == node1x && node2y == node3y){
		node2Count += 1;
	}else if(node2x == node3x && node2y == node1y){
		node2Count += 1;
	}	
	if(node3x == node1x && node3y == node2y){
		node3Count += 1;
	}else if(node3x == node2x && node3y == node1y){
		node3Count += 1;
	}
	unsigned long masterNode, sNode1, sNode2; 
	if(node1Count == 1){
		masterNode = node1; sNode1 = node2; sNode2 = node3;
	}else if(node2Count == 1){
		masterNode = node2; sNode1 = node1; sNode2 = node3;
	}else if(node3Count == 1){
		masterNode = node3; sNode1 = node1; sNode2 = node2;
	}
	*/
//	cout << "Element = " << element1 << endl;
//	cout << "node1 = " << node1 << endl;
//	cout << "node2 = " << node2 << endl;
//	cout << "node3 = " << node3 << endl;
//	cout << "node1x = " << node1x << " node1y = " << node1y << endl;
//	cout << "node2x = " << node2x << " node2y = " << node2y << endl;
//	cout << "node3x = " << node3x << " node3y = " << node3y << endl;
//	cout << "Master Node = " << masterNode << endl;
	
	
//calculating the jacobian matrix based on iPoint being the origin
// commented out on 17/01/2018
/*
	su2double iPointJacobian[2][2], idetJacobian, iPointiJacobian[2][2];
	su2double nodeiPointx, nodeiPointy, nodeis1x, nodeis1y, nodeis2x, nodeis2y;
	su2double cellFaceXi, cellFaceEta;			//cell face position in reference space.
//	nodeiPointx = node[nodeiPoint]->GetCoord(0); nodeiPointy = node[nodeiPoint]->GetCoord(1);
//	nodeis1x = node[nodeiPoints1]->GetCoord(0); nodeis1y = node[nodeiPoints1]->GetCoord(1);
//	nodeis2x = node[nodeiPoints2]->GetCoord(0); nodeis2y = node[nodeiPoints2]->GetCoord(1);
	nodeiPointx = node[masterNode]->GetCoord(0); nodeiPointy = node[masterNode]->GetCoord(1);
	nodeis1x = node[sNode1]->GetCoord(0); nodeis1y = node[sNode1]->GetCoord(1);
	nodeis2x = node[sNode2]->GetCoord(0); nodeis2y = node[sNode2]->GetCoord(1);
	iPointJacobian[0][0] = nodeis1x - nodeiPointx;
	iPointJacobian[0][1] = nodeis2x - nodeiPointx; 
	iPointJacobian[1][0] = nodeis1y - nodeiPointy;
	iPointJacobian[1][1] = nodeis2y - nodeiPointy;
	idetJacobian = 1.0/(iPointJacobian[0][0]*iPointJacobian[1][1] - iPointJacobian[1][0]*iPointJacobian[0][1]);
	iPointiJacobian[0][0] = idetJacobian*(iPointJacobian[1][1]);
	iPointiJacobian[0][1] = -1.0*idetJacobian*(iPointJacobian[0][1]);
	iPointiJacobian[1][0] = -1.0*idetJacobian*(iPointJacobian[1][0]);
	iPointiJacobian[1][1] = idetJacobian*(iPointJacobian[0][0]);
*/
	//calculating the x and y coordinates of the cell face in reference space
//	cellFaceXi = iPointiJacobian[0][0]*(cellFacex-nodeiPointx) + iPointiJacobian[0][1]*(cellFacey-nodeiPointy);
//	cellFaceEta = iPointiJacobian[1][0]*(cellFacex-nodeiPointx) + iPointiJacobian[1][1]*(cellFacey-nodeiPointy);
//	su2double ix, iy, jx,jy;
//	ix = node[iPoint]->GetCoord(0); iy = node[iPoint]->GetCoord(1);
//	jx = node[jPoint]->GetCoord(0); jy = node[jPoint]->GetCoord(1);
//	if(ix == jx){	//vertical cellface
//		cellFaceXi = 0.0; cellFaceEta = 0.5;
//	}
//	if(iy == jy){	//horizontal cellface
//		cellFaceXi = 0.5; cellFaceEta = 0.0;
//	}
//	if(ix != jx && iy != jy){		//diagonal cellFace
//		cellFaceXi = -0.5; cellFaceEta = 0.5;
//	}
//	cout << "iPoint = " << iPoint << " jPoint = " << jPoint << endl;
//	cout << "LEFT HAND SIDE iPointx = " << iPointx << " iPointy = " << iPointy << endl;
//	cout << "LEFT HAND SIDE jPointx = " << jPointx << " jPointy = " << jPointy << endl;	
//	cout << "LEFT HAND SIDE cellFacex = " << cellFacex << " cellFacey = " << cellFacey << endl;
//	cout << "LEFT HAND SIDE cellFaceXi = " << cellFaceXi << " cellFaceEta = " << cellFaceEta << endl;
//	cout << "\n" << endl;

	su2double iPointJacobian[2][2], idetJacobian, iPointiJacobian[2][2];
	su2double nodeiPointx, nodeiPointy, nodeis1x, nodeis1y, nodeis2x, nodeis2y;
	su2double cellFaceXi, cellFaceEta;			//cell face position in reference space.
//	nodeiPointx = node[nodeiPoint]->GetCoord(0); nodeiPointy = node[nodeiPoint]->GetCoord(1);
//	nodeis1x = node[nodeiPoints1]->GetCoord(0); nodeis1y = node[nodeiPoints1]->GetCoord(1);
//	nodeis2x = node[nodeiPoints2]->GetCoord(0); nodeis2y = node[nodeiPoints2]->GetCoord(1);
	nodeiPointx = node[node1]->GetCoord(0); nodeiPointy = node[node1]->GetCoord(1);
	nodeis1x = node[node2]->GetCoord(0); nodeis1y = node[node2]->GetCoord(1);
	nodeis2x = node[node3]->GetCoord(0); nodeis2y = node[node3]->GetCoord(1);
	iPointJacobian[0][0] = nodeis1x - nodeiPointx;
	iPointJacobian[0][1] = nodeis2x - nodeiPointx; 
	iPointJacobian[1][0] = nodeis1y - nodeiPointy;
	iPointJacobian[1][1] = nodeis2y - nodeiPointy;
	idetJacobian = 1.0/(iPointJacobian[0][0]*iPointJacobian[1][1] - iPointJacobian[1][0]*iPointJacobian[0][1] + 1.0E-06); // 25/09/2018 added the 10.0E-16 to prevent division by 0
	iPointiJacobian[0][0] = idetJacobian*(iPointJacobian[1][1]);
	iPointiJacobian[0][1] = -1.0*idetJacobian*(iPointJacobian[0][1]);
	iPointiJacobian[1][0] = -1.0*idetJacobian*(iPointJacobian[1][0]);
	iPointiJacobian[1][1] = idetJacobian*(iPointJacobian[0][0]);
	
	//tranforming the 3 nodes of the element into reference space
	
	//transforming iPoint and jPoint to reference space and finding the distance between them to find Xi and Eta
	iPointXi = iPointiJacobian[0][0]*iPointx + iPointiJacobian[0][1]*iPointy;
	iPointEta = iPointiJacobian[1][0]*iPointx + iPointiJacobian[1][1]*iPointy;
	jPointXi = iPointiJacobian[0][0]*jPointx + iPointiJacobian[0][1]*jPointy;
	jPointEta = iPointiJacobian[1][0]*jPointx + iPointiJacobian[1][1]*jPointy;	
	cellFaceXi = (jPointXi - iPointXi)/2.0;
	cellFaceEta = (jPointEta - iPointEta)/2.0;
	su2double* phiK = new su2double[K];
	for(int i = 0; i < K; i++){
		iter = i + 1;
		if(GEN_ORTHOG){
			if(iter == 1){
				basis_rec = cellFaceXi - 1.0/3.0;
			}else if(iter == 2){
				basis_rec = cellFaceEta - 1.0/3.0;
			}else if(iter == 3){
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;
			}else if(iter == 4){
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;
			}else if(iter == 5){
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;
			}else if(iter == 6){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi - 1.0/10.0;
			}else if(iter == 7){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta - 1.0/30.0;
			}else if(iter == 8){
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta - 1.0/30.0;
			}else if(iter == 9){
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta - 1.0/10.0;
			}else if(iter == 10){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 1.0/15.0;
			}else if(iter == 11){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceEta - 1.0/60.0;
			}else if(iter == 12){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta*cellFaceEta - 1.0/90.0;
			}else if(iter == 13){
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/60.0;
			}else if(iter == 14){
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/15.0;
			}
		}
		if(SHIFTED_LEGENDRE){
			if(iter == 1){
				basis_rec = 2.0*cellFaceXi - 7.0/6.0;
			}else if(iter == 2){
				basis_rec = 2.0*cellFaceEta - 7.0/6.0;
			}else if(iter == 3){
				basis_rec = 6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0;
			}else if(iter == 4){
				basis_rec = (2.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0);
			}else if(iter == 5){
				basis_rec = 6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0;
			}else if(iter == 6){
				basis_rec = 20.0*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 12.0*cellFaceXi - 1.0;
			}else if(iter == 7){
				basis_rec = (6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0)*(2.0*cellFaceEta - 1.0) - 1.0/15.0;
			}else if(iter == 8){
				basis_rec = (6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0)*(2.0*cellFaceXi - 1.0) - 1.0/15.0;
			}else if(iter == 9){
				basis_rec = 20.0*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 12.0*cellFaceEta - 1.0;
			}else if(iter == 10){
				basis_rec = 70.0*cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 140.0*cellFaceXi*cellFaceXi*cellFaceXi + 90.0*cellFaceXi*cellFaceXi - 20.0*cellFaceXi + 1.0;
			}else if(iter == 11){
				basis_rec = (20.0*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 12.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0);
			}else if(iter == 12){
				basis_rec = (6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0)*(6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0);
			}else if(iter == 13){
				basis_rec = (20.0*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 12.0*cellFaceEta - 1.0)*(2.0*cellFaceXi - 1.0);
			}else if(iter == 14){
				basis_rec = 70.0*cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 140.0*cellFaceEta*cellFaceEta*cellFaceEta + 90.0*cellFaceEta*cellFaceEta - 20.0*cellFaceEta + 1.0;
			}	
		}
		if(REGULAR_LEGENDRE){
			if(iter == 1){
				basis_rec = cellFaceXi - 1.0/3.0;
			}else if(iter == 2){
				basis_rec = cellFaceEta - 1.0/3.0;
			}else if(iter == 3){
				basis_rec = 0.5*(3.0*cellFaceXi*cellFaceXi - 1.0) + 1.0/4.0;
			}else if(iter == 4){
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;
			}else if(iter == 5){
				basis_rec = 0.5*(3.0*cellFaceEta*cellFaceEta - 1.0) + 1.0/4.0;
			}else if(iter == 6){
				basis_rec = 0.5*(5.0*cellFaceXi*cellFaceXi*cellFaceXi - 3.0*cellFaceXi) - 3.0/4.0;
			}else if(iter == 7){
				basis_rec = 0.5*(3.0*cellFaceXi*cellFaceXi - 1.0)*cellFaceEta + 7.0/60.0;
			}else if(iter == 8){
				basis_rec = 0.5*(3.0*cellFaceEta*cellFaceEta - 1.0)*cellFaceXi + 7.0/60.0;
			}else if(iter == 9){
				basis_rec = 0.5*(5.0*cellFaceEta*cellFaceEta*cellFaceEta - 3.0*cellFaceEta) - 3.0/4.0;
			}else if(iter == 10){
				basis_rec = (1.0/8.0)*(35.0*cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 3.0) - 1.0/24.0;
			}else if(iter == 11){
				basis_rec = 0.5*(5.0*cellFaceXi*cellFaceXi*cellFaceXi - 3.0*cellFaceXi)*cellFaceEta + 1.0/12.0;
			}else if(iter == 12){
				basis_rec = 0.25*(3.0*cellFaceEta*cellFaceEta - 1.0)*(3.0*cellFaceEta*cellFaceEta - 1.0) - 1.0/40.0;
			}else if(iter == 13){
				basis_rec = 0.5*(5.0*cellFaceEta*cellFaceEta*cellFaceEta - 3.0*cellFaceEta)*cellFaceXi + 1.0/12.0;
			}else if(iter == 14){
				basis_rec = (1.0/8.0)*(35.0*cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 3.0) - 1.0/24.0;
			}		
		}
		phiK[i] = basis_rec;
	}
//	su2double cellFaceXi, cellFaceEta;
//	cellFaceXi = deltax; cellFaceEta = deltay; //commented out on 17/01/2018
//	su2double* phiK = new su2double[K];
/*
	for(int i = 0; i < K; i++){
			iter = i + 1;
			if(iter == 1){
				//epsilon
				//basis_rec = cellFaceXi; 
				basis_rec = cellFaceXi - 1.0/3.0;						// phi_k based on generic orthogonal basis functions
				//basis_rec = 2.0*cellFaceXi - 7.0/6.0;					// phi_k based on shifted legendre basis functions
//				cout << "basis_rec = Xi = " << cellFaceXi << endl;
				//basis_rec = -1.0;
				//basis_rec = 0.0;	//evalated at x = 0.5, y = 0.5; 
				//basis_rec = -2.0/3.0;
			}else if(iter == 2){
				//eta
				//basis_rec = cellFaceEta;
				basis_rec = cellFaceEta - 1.0/3.0;					// phi_k based on generic orthogonal basis functions
				//basis_rec = 2.0*cellFaceEta - 7.0/6.0;						// phi_k based on shifted legendre basis functions
//				cout << "basis_rec = Eta = " << cellFaceEta << endl;
				//basis_rec = -1.0;
				//basis_rec = 0.0;
				//basis_rec = -2.0/3.0;
			}else if(iter == 3){
			//THIRD ORDER
				//epsilon^2
				//basis_rec = cellFaceXi*cellFaceXi;
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;					// phi_k based on generic orthogonal basis functions
				//basis_rec = 6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0;	// phi_k based on shifted legendre basis functions
//				cout << "basis_rec = Xi^2 = " << basis_rec << endl;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 4){
				//epsilon*eta
				//basis_rec = cellFaceXi*cellFaceEta;
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;				// phi_k based on generic orthogonal basis functions
				//basis_rec = (2.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0);		// phi_k based on shifted legendre basis functions
//				cout << "basis_rec = xi*eta = " << basis_rec << endl;
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 5){
				//eta^2
				//basis_rec = cellFaceEta*cellFaceEta;
				basis_rec = cellFaceEta*cellFaceEta - 1.0/6.0;						// phi_k based on generic orthogonal basis functions
				//basis_rec = 6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0;		// phi_k based on shifted legendre basis functions
//				cout << "basis_rec = Eta^2 = " << basis_rec << endl;
				//basis_rec = 1.0;
				//basis_rec = 0.0; 
			}else if(iter == 6){
			//FOURTH ORDER
				//epsilon^3
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi - 1.0/10.0;
				//basis_rec = -1.0/10.0;
			}else if(iter == 7){
				//epsilon^2*eta
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta - 1.0/30.0;
				//basis_rec = -1.0/30.0;
			}else if(iter == 8){
				//epsilon*eta^2
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta - 1.0/30.0;
				//basis_rec = -1.0/30.0;
			}else if(iter == 9){
				//eta^3
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta - 1.0/10.0;
				//basis_rec = -1.0/10.0;
			}else if(iter == 10){
			//FIFTH ORDER 
				//epsilon^4
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 1.0/15.0;
				//basis_rec = -1.0/15.0;
			}else if(iter == 11){
				//epsilon^3*eta
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceEta - 1.0/60.0;
				//basis_rec = -1.0/60.0;	
			}else if(iter == 12){
				//epsilon^2*eta^2
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta*cellFaceEta - 1.0/90.0;
				//basis_rec = -1.0/90.0;
			}else if(iter == 13){
				//epsilon*eta^3		cout << phiK[i] << endl;
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/60.0;
				//basis_rec = -1.0/60.0;
			}else if(iter == 14){
				//eta^4
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/15.0;
				//basis_rec = -1.0/15.0;
			}
		phiK[i] = basis_rec;
	}
	*/
	return phiK;		
}

su2double* CPhysicalGeometry::WenoBasisFunctionAtPointR(int K, unsigned long element2,unsigned long iPoint, unsigned long jPoint){
	su2double basis_rec; 
	su2double iPointx, iPointy, jPointx, jPointy; 
	su2double iPointXi, iPointEta, jPointXi, jPointEta;
	su2double deltax, deltay, cellFacex, cellFacey;					// position of the cell face in reference space
	su2double x,y;
//	unsigned long iPoint, jPoint; 
//	iPoint = edge[iEdge]->GetNode(0); jPoint = edge[iEdge]->GetNode(1);
	iPointx = node[iPoint]->GetCoord(0); iPointy = node[iPoint]->GetCoord(1);
	jPointx = node[jPoint]->GetCoord(0); jPointy = node[jPoint]->GetCoord(1);
	deltax = (jPointx - iPointx)/2.0; deltay = (jPointy - iPointy)/2.0; //finding the cell face distance relative to iPoint (x,y)
	cellFacex = jPointx + deltax; cellFacey = jPointy + deltay;			//finding the position of cell Face in physical space
	int iter = 0;
	unsigned long node1, node2, node3; 
	node1 = elem[element2]->GetNode(0); node2 = elem[element2]->GetNode(1); node3 = elem[element2]->GetNode(2);
	su2double node1x, node1y, node2x, node2y, node3x, node3y;
	node1x = node[node1]->GetCoord(0); node1y = node[node1]->GetCoord(1);
	node2x = node[node2]->GetCoord(0); node2y = node[node2]->GetCoord(1);
	node3x = node[node3]->GetCoord(0); node3y = node[node3]->GetCoord(1);
	bool GEN_ORTHOG = true;
	bool SHIFTED_LEGENDRE = false;
	bool REGULAR_LEGENDRE = false;
	/*
	int node1Count = 0, node2Count = 0, node3Count = 0;
	if(node1x == node2x && node1y == node3y){
		node1Count += 1;
	}else if(node1x == node3x && node1y == node2y){
		node1Count += 1;
	}
	if(node2x == node1x && node2y == node3y){
		node2Count += 1;
	}else if(node2x == node3x && node2y == node1y){
		node2Count += 1;
	}	
	if(node3x == node1x && node3y == node2y){
		node3Count += 1;
	}else if(node3x == node2x && node3y == node1y){
		node3Count += 1;
	}
	unsigned long masterNode, sNode1, sNode2; 
	if(node1Count == 1){
		masterNode = node1; sNode1 = node2; sNode2 = node3;
	}else if(node2Count == 1){
		masterNode = node2; sNode1 = node1; sNode2 = node3;
	}else if(node3Count == 1){
		masterNode = node3; sNode1 = node1; sNode2 = node2;
	}
	*/
//	cout << "Element = " << element1 << endl;
//	cout << "node1 = " << node1 << endl;
//	cout << "node2 = " << node2 << endl;
//	cout << "node3 = " << node3 << endl;
//	cout << "node1x = " << node1x << " node1y = " << node1y << endl;
//	cout << "node2x = " << node2x << " node2y = " << node2y << endl;
//	cout << "node3x = " << node3x << " node3y = " << node3y << endl;
//	cout << "Master Node = " << masterNode << endl;
	
	
//calculating the jacobian matrix based on iPoint being the origin
/*
	su2double jPointJacobian[2][2], idetJacobian, jPointiJacobian[2][2];
	su2double nodejPointx, nodejPointy, nodejs1x, nodejs1y, nodejs2x, nodejs2y;
	su2double cellFaceXi, cellFaceEta;			//cell face position in reference space.
//	nodeiPointx = node[nodeiPoint]->GetCoord(0); nodeiPointy = node[nodeiPoint]->GetCoord(1);
//	nodeis1x = node[nodeiPoints1]->GetCoord(0); nodeis1y = node[nodeiPoints1]->GetCoord(1);
//	nodeis2x = node[nodeiPoints2]->GetCoord(0); nodeis2y = node[nodeiPoints2]->GetCoord(1);
	nodejPointx = node[masterNode]->GetCoord(0); nodejPointy = node[masterNode]->GetCoord(1);
	nodejs1x = node[sNode1]->GetCoord(0); nodejs1y = node[sNode1]->GetCoord(1);
	nodejs2x = node[sNode2]->GetCoord(0); nodejs2y = node[sNode2]->GetCoord(1);
	jPointJacobian[0][0] = nodejs1x - nodejPointx;
	jPointJacobian[0][1] = nodejs2x - nodejPointx; 
	jPointJacobian[1][0] = nodejs1y - nodejPointy;
	jPointJacobian[1][1] = nodejs2y - nodejPointy;
	idetJacobian = 1.0/(jPointJacobian[0][0]*jPointJacobian[1][1] - jPointJacobian[1][0]*jPointJacobian[0][1]);
	jPointiJacobian[0][0] = idetJacobian*(jPointJacobian[1][1]);
	jPointiJacobian[0][1] = -1.0*idetJacobian*(jPointJacobian[0][1]);
	jPointiJacobian[1][0] = -1.0*idetJacobian*(jPointJacobian[1][0]);
	jPointiJacobian[1][1] = idetJacobian*(jPointJacobian[0][0]);
*/
	//calculating the x and y coordinates of the cell face in reference space
//	cellFaceXi = jPointiJacobian[0][0]*(cellFacex-nodejPointx) + jPointiJacobian[0][1]*(cellFacey-nodejPointy);
//	cellFaceEta = jPointiJacobian[1][0]*(cellFacex-nodejPointx) + jPointiJacobian[1][1]*(cellFacey-nodejPointy);
//	su2double ix, iy, jx,jy;
//	ix = node[iPoint]->GetCoord(0); iy = node[iPoint]->GetCoord(1);
//	jx = node[jPoint]->GetCoord(0); jy = node[jPoint]->GetCoord(1);
//	if(ix == jx){	//vertical cellface
//		cellFaceXi = 0.0; cellFaceEta = 0.5;
//	}
//	if(iy == jy){	//horizontal cellface
//		cellFaceXi = 0.5; cellFaceEta = 0.0;
//	}
//	if(ix != jx && iy != jy){		//diagonal cellFace
//		cellFaceXi = -0.5; cellFaceEta = 0.5;
//	}
	
//	cout << "RIGHT HAND SIDE iPointx = " << iPointx << " iPointy = " << iPointy << endl;
//	cout << "RIGHT HAND SIDE jPointx = " << jPointx << " jPointy = " << jPointy << endl;
//	cout << "RIGHT HAND SIDE cellFacex = " << cellFacex << " cellFacey = " << cellFacey << endl;
//	cout << "RIGHT HAND SIDE cellFaceXi = " << cellFaceXi << " cellFaceEta = " << cellFaceEta << endl;
//	cout << "\n" << endl;

	su2double iPointJacobian[2][2], idetJacobian, iPointiJacobian[2][2];
	su2double nodeiPointx, nodeiPointy, nodeis1x, nodeis1y, nodeis2x, nodeis2y;
	su2double cellFaceXi, cellFaceEta;			//cell face position in reference space.
//	nodeiPointx = node[nodeiPoint]->GetCoord(0); nodeiPointy = node[nodeiPoint]->GetCoord(1);
//	nodeis1x = node[nodeiPoints1]->GetCoord(0); nodeis1y = node[nodeiPoints1]->GetCoord(1);
//	nodeis2x = node[nodeiPoints2]->GetCoord(0); nodeis2y = node[nodeiPoints2]->GetCoord(1);
	nodeiPointx = node[node1]->GetCoord(0); nodeiPointy = node[node1]->GetCoord(1);
	nodeis1x = node[node2]->GetCoord(0); nodeis1y = node[node2]->GetCoord(1);
	nodeis2x = node[node3]->GetCoord(0); nodeis2y = node[node3]->GetCoord(1);
	iPointJacobian[0][0] = nodeis1x - nodeiPointx;
	iPointJacobian[0][1] = nodeis2x - nodeiPointx; 
	iPointJacobian[1][0] = nodeis1y - nodeiPointy;
	iPointJacobian[1][1] = nodeis2y - nodeiPointy;
	idetJacobian = 1.0/(iPointJacobian[0][0]*iPointJacobian[1][1] - iPointJacobian[1][0]*iPointJacobian[0][1] + 1.0E-06); // 25/09/2018 added 10.0E-16 to prevent division by 0
	iPointiJacobian[0][0] = idetJacobian*(iPointJacobian[1][1]);
	iPointiJacobian[0][1] = -1.0*idetJacobian*(iPointJacobian[0][1]);
	iPointiJacobian[1][0] = -1.0*idetJacobian*(iPointJacobian[1][0]);
	iPointiJacobian[1][1] = idetJacobian*(iPointJacobian[0][0]);
	
	//tranforming the 3 nodes of the element into reference space
	
	//transforming iPoint and jPoint to reference space and finding the distance between them to find Xi and Eta
	iPointXi = iPointiJacobian[0][0]*iPointx + iPointiJacobian[0][1]*iPointy;
	iPointEta = iPointiJacobian[1][0]*iPointx + iPointiJacobian[1][1]*iPointy;
	jPointXi = iPointiJacobian[0][0]*jPointx + iPointiJacobian[0][1]*jPointy;
	jPointEta = iPointiJacobian[1][0]*jPointx + iPointiJacobian[1][1]*jPointy;	
	cellFaceXi = (iPointXi - jPointXi)/2.0;
	cellFaceEta = (iPointEta - jPointEta)/2.0;
//	su2double cellFaceXi, cellFaceEta;
//	cellFaceXi = deltax; cellFaceEta = deltay;
//	cout << "RHS deltax = " << deltax << " deltay = " << deltay << endl;
	su2double* phiK = new su2double[K];
	for(int i = 0; i < K; i++){
		iter = i + 1;
		if(GEN_ORTHOG){
			if(iter == 1){
				basis_rec = cellFaceXi - 1.0/3.0;
			}else if(iter == 2){
				basis_rec = cellFaceEta - 1.0/3.0;
			}else if(iter == 3){
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;
			}else if(iter == 4){
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;
			}else if(iter == 5){
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;
			}else if(iter == 6){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi - 1.0/10.0;
			}else if(iter == 7){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta - 1.0/30.0;
			}else if(iter == 8){
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta - 1.0/30.0;
			}else if(iter == 9){
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta - 1.0/10.0;
			}else if(iter == 10){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 1.0/15.0;
			}else if(iter == 11){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceEta - 1.0/60.0;
			}else if(iter == 12){
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta*cellFaceEta - 1.0/90.0;
			}else if(iter == 13){
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/60.0;
			}else if(iter == 14){
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/15.0;
			}
		}
		if(SHIFTED_LEGENDRE){
			if(iter == 1){
				basis_rec = 2.0*cellFaceXi - 7.0/6.0;
			}else if(iter == 2){
				basis_rec = 2.0*cellFaceEta - 7.0/6.0;
			}else if(iter == 3){
				basis_rec = 6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0;
			}else if(iter == 4){
				basis_rec = (2.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0);
			}else if(iter == 5){
				basis_rec = 6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0;
			}else if(iter == 6){
				basis_rec = 20.0*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 12.0*cellFaceXi - 1.0;
			}else if(iter == 7){
				basis_rec = (6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0)*(2.0*cellFaceEta - 1.0) - 1.0/15.0;
			}else if(iter == 8){
				basis_rec = (6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0)*(2.0*cellFaceXi - 1.0) - 1.0/15.0;
			}else if(iter == 9){
				basis_rec = 20.0*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 12.0*cellFaceEta - 1.0;
			}else if(iter == 10){
				basis_rec = 70.0*cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 140.0*cellFaceXi*cellFaceXi*cellFaceXi + 90.0*cellFaceXi*cellFaceXi - 20.0*cellFaceXi + 1.0;
			}else if(iter == 11){
				basis_rec = (20.0*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 12.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0);
			}else if(iter == 12){
				basis_rec = (6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0)*(6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0);
			}else if(iter == 13){
				basis_rec = (20.0*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 12.0*cellFaceEta - 1.0)*(2.0*cellFaceXi - 1.0);
			}else if(iter == 14){
				basis_rec = 70.0*cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 140.0*cellFaceEta*cellFaceEta*cellFaceEta + 90.0*cellFaceEta*cellFaceEta - 20.0*cellFaceEta + 1.0;
			}	
		}
		if(REGULAR_LEGENDRE){
			if(iter == 1){
				basis_rec = cellFaceXi - 1.0/3.0;
			}else if(iter == 2){
				basis_rec = cellFaceEta - 1.0/3.0;
			}else if(iter == 3){
				basis_rec = 0.5*(3.0*cellFaceXi*cellFaceXi - 1.0) + 1.0/4.0;
			}else if(iter == 4){
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;
			}else if(iter == 5){
				basis_rec = 0.5*(3.0*cellFaceEta*cellFaceEta - 1.0) + 1.0/4.0;
			}else if(iter == 6){
				basis_rec = 0.5*(5.0*cellFaceXi*cellFaceXi*cellFaceXi - 3.0*cellFaceXi) - 3.0/4.0;
			}else if(iter == 7){
				basis_rec = 0.5*(3.0*cellFaceXi*cellFaceXi - 1.0)*cellFaceEta + 7.0/60.0;
			}else if(iter == 8){
				basis_rec = 0.5*(3.0*cellFaceEta*cellFaceEta - 1.0)*cellFaceXi + 7.0/60.0;
			}else if(iter == 9){
				basis_rec = 0.5*(5.0*cellFaceEta*cellFaceEta*cellFaceEta - 3.0*cellFaceEta) - 3.0/4.0;
			}else if(iter == 10){
				basis_rec = (1.0/8.0)*(35.0*cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 30.0*cellFaceXi*cellFaceXi + 3.0) - 1.0/24.0;
			}else if(iter == 11){
				basis_rec = 0.5*(5.0*cellFaceXi*cellFaceXi*cellFaceXi - 3.0*cellFaceXi)*cellFaceEta + 1.0/12.0;
			}else if(iter == 12){
				basis_rec = 0.25*(3.0*cellFaceEta*cellFaceEta - 1.0)*(3.0*cellFaceEta*cellFaceEta - 1.0) - 1.0/40.0;
			}else if(iter == 13){
				basis_rec = 0.5*(5.0*cellFaceEta*cellFaceEta*cellFaceEta - 3.0*cellFaceEta)*cellFaceXi + 1.0/12.0;
			}else if(iter == 14){
				basis_rec = (1.0/8.0)*(35.0*cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 30.0*cellFaceEta*cellFaceEta + 3.0) - 1.0/24.0;
			}		
		}
		phiK[i] = basis_rec;
	}
	/*
	for(int i = 0; i < K; i++){
			iter = i + 1;
			if(iter == 1){
				//epsilon
				//basis_rec = cellFaceXi;
				basis_rec = cellFaceXi - 1.0/3.0;
				//basis_rec = 2.0*cellFaceXi - 7.0/6.0;					// phi_k based on shifted legendre basis functions
				//basis_rec = -1.0;
				//basis_rec = 0.0;	//evalated at x = 0.5, y = 0.5; 
				//basis_rec = -2.0/3.0;
			}else if(iter == 2){
				//eta
				//basis_rec = cellFaceEta;
				basis_rec = cellFaceEta - 1.0/3.0;
				//basis_rec = 2.0*cellFaceEta - 7.0/6.0;					// phi_k based on shifted legendre basis functions
				//basis_rec = -1.0;
				//basis_rec = 0.0;
				//basis_rec = -2.0/3.0;
			}else if(iter == 3){
			//THIRD ORDER
				//epsilon^2
				//basis_rec = cellFaceXi*cellFaceXi;
				basis_rec = cellFaceXi*cellFaceXi - 1.0/6.0;
				//basis_rec = 6.0*cellFaceXi*cellFaceXi - 6.0*cellFaceXi + 1.0;	// phi_k based on shifted legendre basis functions
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 4){
				//epsilon*eta
				//basis_rec = cellFaceXi*cellFaceEta;
				basis_rec = cellFaceXi*cellFaceEta - 1.0/12.0;
				//basis_rec = (2.0*cellFaceXi - 1.0)*(2.0*cellFaceEta - 1.0); // phi_k based on shifted legendre basis functions
				//basis_rec = 1.0;
				//basis_rec = 0.0;
			}else if(iter == 5){
				//eta^2
				//basis_rec = cellFaceEta*cellFaceEta;
				basis_rec = cellFaceEta*cellFaceEta - 1.0/6.0;
				//basis_rec = 6.0*cellFaceEta*cellFaceEta - 6.0*cellFaceEta + 1.0;	// phi_k based on shifted legendre basis functions
				//basis_rec = 1.0;
				//basis_rec = 0.0; 
			}else if(iter == 6){
			//FOURTH ORDER
				//epsilon^3
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi - 1.0/10.0;
				//basis_rec = -1.0/10.0;
			}else if(iter == 7){
				//epsilon^2*eta
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta - 1.0/30.0;
				//basis_rec = -1.0/30.0;
			}else if(iter == 8){
				//epsilon*eta^2
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta - 1.0/30.0;
				//basis_rec = -1.0/30.0;
			}else if(iter == 9){
				//eta^3
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta - 1.0/10.0;
				//basis_rec = -1.0/10.0;
			}else if(iter == 10){
			//FIFTH ORDER 
				//epsilon^4
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceXi - 1.0/15.0;
				//basis_rec = -1.0/15.0;
			}else if(iter == 11){
				//epsilon^3*eta
				basis_rec = cellFaceXi*cellFaceXi*cellFaceXi*cellFaceEta - 1.0/60.0;
				//basis_rec = -1.0/60.0;	
			}else if(iter == 12){
				//epsilon^2*eta^2
				basis_rec = cellFaceXi*cellFaceXi*cellFaceEta*cellFaceEta - 1.0/90.0;
				//basis_rec = -1.0/90.0;
			}else if(iter == 13){
				//epsilon*eta^3		cout << phiK[i] << endl;
				basis_rec = cellFaceXi*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/60.0;
				//basis_rec = -1.0/60.0;
			}else if(iter == 14){
				//eta^4
				basis_rec = cellFaceEta*cellFaceEta*cellFaceEta*cellFaceEta - 1.0/15.0;
				//basis_rec = -1.0/15.0;
			}
		phiK[i] = basis_rec;
	}
	*/
	return phiK;	
}

su2double** CPhysicalGeometry::SetDegMatrix(int J, int K, int N, su2double** xCoord, su2double** yCoord){
	// this function will take in the Phi_k and assemble the A_ij matrix and return the (A_ij^T x A_ij)^inverse * A_ij^(T) 
	// for the degrees of freedom calculation during the reconstruction step of in solver_direct_mean.cpp 
	// assembling the Aij matrix of the reconstruction part
	// Aij is the cell average of the basis function phi_k over cell j in the stencil
	// int NP = 4, caseFunc;
	int NP = 4, caseFunc;
	su2double A[J][K], A1[J][K], A2[J][K], A3[J][K];
	su2double B[K][J], B1[K][J], B2[K][J], B3[K][J];
	su2double C[K][K], C1[K][K], C2[K][K], C3[K][K];
	su2double x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12;
	su2double y1, y2, y3, y4, y5, y6, y7, y8, y9, y10, y11, y12;
	su2double x, y, x_d1, x_d2, x_d3, y_d1, y_d2, y_d3; 
	su2double matrix_container[K][J], matrix_container1[K][J], matrix_container2[K][J], matrix_container3[K][J];	// only if weno safety robust factor  == 2 otherwise have to change J to equal 2*K
	su2double ratio, ratio1, ratio2, ratio3, a, a1, a2, a3;
	su2double invA[K][K], invA1[K][K], invA2[K][K], invA3[K][K], product[K][J], product1[K][J], product2[K][J], product3[K][J];
	su2double** degMat = 0;
	su2double integ = 0.0, integ1 = 0.0, integ2 = 0.0, integ3 = 0.0;
	// Variables init for SVD of Matrix A for the degrees of freedom calculation
	su2double U[J][K], Sigma[K][K], VT[K][K]; // where SVD of A = U*W*V(T) 
	int m = WENO3_J, n = WENO3_K, lda = LDA, ldu = LDU, ldvt = LDVT, info, lwork;
	int m1 = WENO3_J, n1 = WENO3_K, lda1 = LDA, ldu1 = LDU, ldvt1 = LDVT, info1, lwork1;
	int m2 = WENO3_J, n2 = WENO3_K, lda2 = LDA, ldu2 = LDU, ldvt2 = LDVT, info2, lwork2;
	int m3 = WENO3_J, n3 = WENO3_K, lda3 = LDA, ldu3 = LDU, ldvt3 = LDVT, info3, lwork3;
	double* wkopt;
	double* work;
	int dummyVar = 0;
	char jobu = 'U', jobvt = 'V';
	//DEBUGGING CONTAINERS FOR SVD CHECK TO SEE IF BEING READ IN AND STORED CORRECTLY FOR ROW MAJOR ORDER
	int TESTm = TESTM, TESTn = TESTN, TESTlda = TESTLDA, TESTldu = TESTLDU, TESTldvt = TESTLDVT, TESTinfo; 
	double TESTsuperb[min(TESTm, TESTn) - 1];
	double TESTs[TESTn], TESTu[TESTldu*TESTm], TESTvt[TESTldvt*TESTn];
	double TESTa[TESTlda*TESTm] = {
		8.79,  9.93,  9.83, 5.45,  3.16,
		6.11,  6.91,  5.04, -0.27,  7.98,
		-9.15, -7.93,  4.86, 4.85,  3.01,
		9.57,  1.64,  8.83, 0.74,  5.80,
		-3.49,  4.02,  9.80, 10.00,  4.27,
		9.84,  0.15, -8.99, -6.02, -5.31
	};
	//cout<<setprecision(2)<<fixed; //FOR DEBUGGING ONLY
	/*
	cout << "Printing the Matrix to be decomposed" << endl;
	cout << "\n";
	for(int i = 0; i < TESTm; i++){
		for(int j = 0; j < TESTn; j++){
			cout << TESTa[i*TESTlda+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	cout << "Printing the Matrix to be deconmped in vector form" << endl;
	for(int i = 0; i < 30; i++){
		cout << TESTa[i] << " , ";
	}
	cout << "\n" << endl;
	*/
	//Local arrays
	su2double s[WENO3_K], u[LDU*WENO3_J], vt[LDVT*WENO3_K], aSVD[LDA*WENO3_K], superb[min(J,K)-1];
	su2double s1[WENO3_K], u1[LDU*WENO3_J], vt1[LDVT*WENO3_K], aSVD1[LDA*WENO3_K], superb1[min(J,K)-1];
	su2double s2[WENO3_K], u2[LDU*WENO3_J], vt2[LDVT*WENO3_K], aSVD2[LDA*WENO3_K], superb2[min(J,K)-1];
	su2double s3[WENO3_K], u3[LDU*WENO3_J], vt3[LDVT*WENO3_K], aSVD3[LDA*WENO3_K], superb3[min(J,K)-1];
	for(int i = 0; i < J*K; i++){
		aSVD[i] = 0.0; aSVD1[i] = 0.0; aSVD2[i] = 0.0; aSVD3[i] = 0.0;
	}
	// --------------------------------------------------//
	//------------------CALCULATIONS BEGIN---------------//
	// --------------------------------------------------//
//	degMat = new su2double*[K];
//	for(int i = 0; i < K; i++){
//		degMat[i] = new su2double[4*J];
//		for(int j = 0; j < 4*J; j++){
//			degMat[i][j] = 0.0;
//		}
//	}
	degMat = new su2double*[J];
	
	//07/03/2018 Correction regarding dimensions initiallu U was thought to be an mxn matrix when it is infact an mxm matrix where m=J
	for(int i = 0; i < J; i++){
		degMat[i] = new su2double[16*K]; // each A matrix is decomposed into mxm + mxn + nxn = 4*n columns x 4 stencils = 16*n columns
		for(int j = 0; j < 16*K; j++){	// 28/08/2018 was 12*K before
			degMat[i][j] = 0.0;
		}
	}
	
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			A[i][j] = 0.0; A1[i][j] = 0.0; A2[i][j] = 0.0; A3[i][j] = 0.0;
			B[j][i] = 0.0; B1[j][i] = 0.0; B2[j][i] = 0.0; B3[j][i] = 0.0;
			product[K][J] = 0.0; product1[K][J] = 0.0; product2[K][J] = 0.0; product3[K][J] = 0.0;
		}
	}
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			C[i][j] = 0.0; C1[i][j] = 0.0; C2[i][j] = 0.0; C3[i][j] = 0.0;
			invA[i][j] = 0.0; invA1[i][j] = 0.0; invA2[i][j] = 0.0; invA3[i][j] = 0.0;
		}
	}
	
	su2double** xw = TriGaussPoints(N); 
	

	
	for(int i = 0; i < J; i++){
        //Central stencil Assignment
        
		x1 = xCoord[i][0]; x2 = xCoord[i][1]; x3 = xCoord[i][2];
        y1 = yCoord[i][0]; y2 = yCoord[i][1]; y3 = yCoord[i][2];
        //x1 = 0.0; x2 = 1.0; x3 = 0.0;
		//y1 = 0.0; y2 = 0.0; y3 = 1.0;
		//Face 1 Directional Stencil Assignment
        x4 = xCoord[i+J][0]; x5 = xCoord[i+J][1]; x6 = xCoord[i+J][2];
        y4 = yCoord[i+J][0]; y5 = yCoord[i+J][1]; y6 = yCoord[i+J][2];
        //Face 2 Directional Stencil Assignment
        x7 = xCoord[i+2*J][0]; x8 = xCoord[i+2*J][1]; x9 = xCoord[i+2*J][2];
        y7 = yCoord[i+2*J][0]; y8 = yCoord[i+2*J][1]; y9 = yCoord[i+2*J][2];
        //Face 3 Directional Stencil Assignment
        x10 = xCoord[i+3*J][0]; x11 = xCoord[i+3*J][1]; x12 = xCoord[i+3*J][2];
        y10 = yCoord[i+3*J][0]; y11 = yCoord[i+3*J][1]; y12 = yCoord[i+3*J][2];
		
		//---------------------------------------------------------------------//
		//-----------PRINTING THE X1 X2 X3 Y1 Y2 Y3 COORDINATES----------------//
		//---------------------------------------------------------------------//
//		cout << "x1 = " << x1 << " x2 = " << x2 << " x3 = " << x3 << endl;
//		cout << "y1 = " << y1 << " y2 = " << y2 << " y3 = " << y3 << endl;
		
        for(int j = 0; j < K; j++){
            for(int k = 0; k < NP; k++){
                //the output of this for loop will be t)he integral of Psi_{k}Program exited with return code: 0

                // the x1,x2,x3,y1,y2,y3 should be the coordinates of the unit triangle
                // of cell 0 after the tranformation from the physical coordinate system to
                // the reference coordinate system
                x = x1*(1.0 - xw[k][0] - xw[k][1]) + x2*xw[k][0] + x3*xw[k][1];
                y = y1*(1.0 - xw[k][0] - xw[k][1]) + y2*xw[k][0] + y3*xw[k][1];
//				cout << "x = " << x << " y = " << y <<  endl;
                x_d1 = x4*(1.0 - xw[k][0] - xw[k][1]) + x5*xw[k][0] + x6*xw[k][1];
                y_d1 = y4*(1.0 - xw[k][0] - xw[k][1]) + y5*xw[k][0] + y6*xw[k][1];
                x_d2 = x7*(1.0 - xw[k][0] - xw[k][1]) + x8*xw[k][0] + x9*xw[k][1];
                y_d2 = y7*(1.0 - xw[k][0] - xw[k][1]) + y8*xw[k][0] + y9*xw[k][1];
                x_d3 = x10*(1.0 - xw[k][0] - xw[k][1]) + x11*xw[k][0] + x12*xw[k][1];
                y_d3 = y10*(1.0 - xw[k][0] - xw[k][1]) + y11*xw[k][0] + y12*xw[k][1];
                caseFunc = j+1;
                integ += 0.5*SetWenoBasisFunction(caseFunc, x, y)*xw[k][2];
                integ1 += 0.5*SetWenoBasisFunction(caseFunc, x_d1, y_d1)*xw[k][2];
                integ2 += 0.5*SetWenoBasisFunction(caseFunc, x_d2, y_d2)*xw[k][2];
                integ3 += 0.5*SetWenoBasisFunction(caseFunc, x_d3, y_d3)*xw[k][2];
            }
			//14/02/2018 removed the 2.0*integ as per WENO paper in Computers and fluids
			// ENTRY 26/09/2018
			// At some point I readded the 2.0*integ to be consistent with the Communication in Computational Physics paper (2010)
			// As per the Computers & Fluids paper 2018, The formula to calculate Aij only involves the integral of the basis function over the cell in the stencil (THERE IS NO DIVIDING BY CELL AREA)
			// The division by cell area comes when assembling the bj vector during the solution reconstruction step. 
			// I did a quick test to see the effect of this scaling on the values of the matrices
			// If Aij = 2.0*Aij then the eigenvalues in the matrix post the SVD are twice as large as they should be.
			// As of now I am getting extremely large reconstructed values in certain parts of the domain, this could be one reason why.
			// So I am removing the 2.0 infront of the integ. 
			A[i][j] = integ; A1[i][j] = integ1; A2[i][j] = integ2; A3[i][j] = integ3; //no 2.0 before, initially I forgot to divide by the reference cell area. when transformed to reference space, triangles form unit triangle which have area = 0.5/ diving by area = 1/area = 1/0.5 = 2.0; 
			integ = 0.0; integ1 = 0.0; integ2 = 0.0; integ3 = 0.0;
        }
    }
	/*
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){ 
			cout << A[i][j] << " , ";
		}
		cout << "\n";
	}
	exit(0);
	*/
	// copying the element sof matrix A into vector aSVD
	int svdIter = 0;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			aSVD[svdIter] = A[i][j]; aSVD1[svdIter] = A1[i][j]; aSVD2[svdIter] = A2[i][j]; aSVD3[svdIter] = A3[i][j];
			svdIter += 1; 
		}
	}
	/*
	cout << "svdIter = " << svdIter << endl;
	cout << "Printing the A Matrix " << endl;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			cout << A[i][j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	cout << "Printing A matrix in vector form" << endl;
	for(int i = 0; i < 50; i++){
		cout << aSVD[i] << " , ";
	}
	cout << "\n" << endl;
	*/
	//lwork = -1; 
	info  = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', m , n , aSVD , lda , s , u , ldu , vt , ldvt , superb );
	info1 = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', m1, n1, aSVD1, lda1, s1, u1, ldu1, vt1, ldvt1, superb1);
	info2 = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', m2, n2, aSVD2, lda2, s2, u2, ldu2, vt2, ldvt2, superb2);
	info3 = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', m3, n3, aSVD3, lda3, s3, u3, ldu3, vt3, ldvt3, superb3);
	
	//-----------EXAMPLE CODE FOR USING LAPACKE ---------//
	//cout << "LAPACKE dgesvd (row-major, high level) Example\n" << endl;
	//TESTinfo = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', TESTm, TESTn, TESTa, TESTlda, TESTs, TESTu, TESTldu, TESTvt, TESTldvt, TESTsuperb);
	/*
	if(TESTinfo > 0 ){
		cout << "The algorithm computing SVD failed to converge. \n";
	}
	//------------EXAMPLE CODE-------------//
	cout << "Printing the singular values of the test matrix" << endl;
	for(int i = 0; i < 1; i++){
		for(int j = 0; j < TESTn; j++){
			cout << TESTs[i+j] << " , ";
		}
	}
	cout << "\n" << endl;
	cout << "Printing the Left Singular Vectors (stored columnwise)" << endl;
	for(int i = 0; i < TESTm; i++){
		for(int j = 0; j < TESTn; j++){
			cout << TESTu[i*TESTldu+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	cout << "Printing the Right singular vectors (stored rowwise)" << endl;
	for(int i = 0; i < TESTn; i++){
		for(int j = 0; j < TESTn;j++){
			cout << TESTvt[i*TESTldvt+j] << " , ";
		}
		cout << "\n";
	}
	*/
	/*
	cout << "\n" << endl;
	cout << "Printing the singular values of the A matrix" << endl;
	for(int i = 0; i < 1; i++){
		for(int j = 0; j < n; j++){
			cout << s[i+j] << " , ";
		}
	}
	//M = 6, n = 5 and a = mxn, lda = n, ldu = m, ldvt = n
	//J = 10, K = 5 and A = JxK
	cout << "\n" << endl;
	cout << "Printing the Left Singular Vectors (stored Column wise)" << endl;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			cout << u[i*J+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	cout << "Printing the Right singular vectors (stored rowwise)" << endl;
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << vt[i*K+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	
	//exit(0);
	*/
	//lwork = (int*)wkopt;
	//work = (double*)malloc( lwork*sizeof(double));
	//info = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, 'A', 'A', m, n, aSVD, lda, s, u, ldu, vt, ldvt, work);
//	info = LAPACKE_dgesvd(LAPACK_ROW_MAJOR, "All", "All", &m, &n, aSVD, &lda, s, u, &ldu, vt, &ldvt, work, &lwork, &info);

//	lapack_int LAPACKE_dgesvd( int matrix_layout, char jobu, char jobvt,
//                           lapack_int m, lapack_int n, double* a,
 //                          lapack_int lda, double* s, double* u, lapack_int ldu,
 //                          double* vt, lapack_int ldvt, double* superb );
//
/*	
	cout << "-------------------------------------------" << endl;
	cout << "PRINTING THE A MATRIX TO CHECK CALCULATIONS" << endl;
	cout << "-------------------------------------------" << endl;
	
	
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			cout << A[i][j] << " , ";
		}
	}
*/
//		
	// calculating the Transpose of matrix Aij
    for(int i = 0; i < K; i++){
        for(int j = 0; j < J; j++){
            B[i][j] = A[j][i];
            B1[i][j] = A1[j][i];
            B2[i][j] = A2[j][i];
            B3[i][j] = A3[j][i];
        }
    }
//	
/*
	cout << "----------------------------------------------" << endl;
	cout << "PRINTING THE A(T) MATRIX TO CHECK CALCULATIONS" << endl;
	cout << "----------------------------------------------" << endl;
//	
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < J; j++){
			cout << B[i][j] << " , ";
		}
		cout << "\n";
	}
*/
//	;	
    // calculating the Product A(T)*A
    for(int i = 0; i < K; i++){
        for(int j = 0; j < K; j++){
            C[i][j] = 0.0; C1[i][j] = 0.0; C2[i][j] = 0.0; C3[i][j] = 0.0;
            for(int k = 0; k < J; k++){
                C[i][j] += B[i][k]*A[k][j]; C1[i][j] += B1[i][k]*A1[k][j]; C2[i][j] += B2[i][k]*A2[k][j]; C3[i][j] += B3[i][k]*A3[k][j];
            }
        }
    }
//
/*	
	cout << "------------------------------------------------" << endl;
	cout << "PRINTING THE A(T)*A MATRIX TO CHECK CALCULATIONS" << endl;
	cout << "------------------------------------------------" << endl;
//	
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << C[i][j] << " , ";
		}
		cout << "\n";
	}
*/		
//	   
	// calculating the inverse of (A(T)*A)
    //creating a placeholder matrix to do the calculations
    
    for(int i = 0; i < K; i++){
        for(int j = 0; j < K; j++){
            matrix_container[i][j] = C[i][j]; // placing the calculation A^(T)*A in a place holder matrix
            matrix_container1[i][j] = C1[i][j];
            matrix_container2[i][j] = C2[i][j];
            matrix_container3[i][j] = C3[i][j];
        }
    }
    
    for(int i = 0; i < K; i++){
        for(int j = K; j < J; j++){
            if(i ==(j-K)){
                matrix_container[i][j] = 1.0; matrix_container1[i][j] = 1.0; matrix_container2[i][j] = 1.0; matrix_container3[i][j] = 1.0;
            }else{
                matrix_container[i][j] = 0.0; matrix_container1[i][j] = 0.0; matrix_container2[i][j] = 0.0; matrix_container3[i][j] = 0.0;
            }
        }
    }
    
    for(int i = 0; i < K; i++){
        for(int j = 0; j < K; j++){
            if(i!=j){
                ratio = matrix_container[j][i]/matrix_container[i][i];
                ratio1 = matrix_container1[j][i]/matrix_container1[i][i];
                ratio2 = matrix_container2[j][i]/matrix_container2[i][i];
                ratio3 = matrix_container3[j][i]/matrix_container3[i][i];
                for(int k = 0; k < J; k++){
                    matrix_container[j][k] -= ratio*matrix_container[i][k];
                    matrix_container1[j][k] -= ratio1*matrix_container1[i][k];
                    matrix_container2[j][k] -= ratio2*matrix_container2[i][k];
                    matrix_container3[j][k] -= ratio3*matrix_container3[i][k];
                }
            }
        }
    }
    
    for(int i = 0; i < K; i++){
        a = matrix_container[i][i]; a1 = matrix_container1[i][i]; a2 = matrix_container2[i][i]; a3 = matrix_container3[i][i];
        for(int j = 0; j < J; j++){
            matrix_container[i][j] /= a; matrix_container1[i][j] /= a1; matrix_container2[i][j] /= a2; matrix_container3[i][j] /= a3;
        }
    }
    
    //nested for loop to obtain the inverse matrix
    for(int i = 0; i < K; i++){
        for(int j = 0; j < K; j++){
            invA[i][j] = matrix_container[i][j+K];
            invA1[i][j] = matrix_container1[i][j+K];
            invA2[i][j] = matrix_container2[i][j+K];
            invA3[i][j] = matrix_container3[i][j+K];
        }
    }
/*	
	cout << "-------------------------------------------------------" << endl;
	cout << "PRINTING THE (A(T)*A)^(-1) MATRIX TO CHECK CALCULATIONS" << endl;
	cout << "-------------------------------------------------------" << endl;
	cout << "\n";
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << invA[i][j] << " , ";
		}
		cout << "\n";
	}
*/		
//	cout << "\n";    
    // calculating the product (A(T)*A)^(-1)*A(T))
    for(int i = 0; i < K; i++){
        for(int j = 0; j < J; j++){
            product[i][j] = 0.0; product1[i][j] = 0.0; product2[i][j] = 0.0; product3[i][j] = 0.0;
            for(int k = 0; k < K; k++){
                product[i][j] += invA[i][k]*B[k][j];    // (A(T)*A)^(-1)*A(T)
                product1[i][j] += invA1[i][k]*B1[k][j];
                product2[i][j] += invA2[i][k]*B2[k][j];
                product3[i][j] += invA3[i][k]*B3[k][j];
            }
        }
    }
/*
	cout << "------------------------------------------------------------" << endl;
	cout << "PRINTING THE (A(T)*A)^(-1)*A(T) MATRIX TO CHECK CALCULATIONS" << endl;
	cout << "------------------------------------------------------------" << endl;
	cout << "\n";
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < J; j++){
			cout << product[i][j] << " , ";
		}
		cout << "\n";
	}
		
	cout << "\n"; 
*/
	//----------------------------------------------------------//
	//-----------ATTEMPTING THE SVD CALCULATIONS----------------//
	//---------------------------------------------------------//
	//void LAPACK_dgesvd( char* jobu, char* jobvt, lapack_int* m, lapack_int* n,
    //                double* a, lapack_int* lda, double* s, double* u,
    //                lapack_int* ldu, double* vt, lapack_int* ldvt, double* work,
    //                lapack_int* lwork, lapack_int *info );
//    cout << "DGESVD Calculations" << endl;
//	info = -1; 
//	dummyVar = LAPACKE_dgesvd(info, jobu, jobvt,  m, n, aSVD, lda, s, u, ldu, vt, ldvt, wkopt);
	//LAPACKE_dgesvd("All", "All", &m, &n, aSVD, &lda, s, u, &ldu, vt, &ldvt, &wkopt, &lwork, &info); // SVD call
	//info = (int)wkopt;
//	work = (su2double*)malloc(lwork*sizeof(su2double));
	// Compute SVD 
//	dummyVar = LAPACKE_dgesvd(info, jobu, jobvt, m, n, aSVD, lda, s, u, ldu, vt, ldvt, work);
	// Check for convergence 
	//if(info > 0){
	//	cout << "The algorithm computing SVD failed to converge. \n" << endl; 
	//}
	/*
	cout << "-----------------------------------------------------------" << endl;
	cout << "Printing the Parent Degrees of Freedom Variation Matrix Aij" << endl;
	cout << "-----------------------------------------------------------" << endl;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			cout << A[i][j] << " ";
		}
		cout << "\n";
	}
	
	cout << "-------------------------------------- " << endl;
	cout << "Printing the SVD Decomposition U Matrix" << endl;
	cout << "---------------------------------------" << endl;	
	
	for(int i = 0; i < J; i++){
		for(int j =0 ; j < K; j++){
			cout << u[i+j*lda] << " ";
		}
		cout << "\n";
	}
	
	cout << "--------------------------------------------- " << endl;
	cout << "Printing the SVD Decomposition Singular Values" << endl;
	cout << "----------------------------------------------" << endl;	
	
	for(int i = 0; i < K; i++){
		cout << s[i] << " , ";
	}
	cout << "\n" << endl;
	
	cout << "----------------------------------------" << endl;
	cout << "Printing the SVD Decomposition VT Matrix" << endl;
	cout << "----------------------------------------" << endl;	
	
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << vt[i+j*lda] << " ";
		}
		cout << "\n";
	}
	
	*/
	//for loop placing the information from the product matrices into one matrix which will be returned for future calculations
//    for(int i = 0; i < K; i++){
//        for(int j = 0; j < J; j++){
//            degMat[i][j] = product[j][i];
//            degMat[i][j+J] = product1[j][i];
//            degMat[i][j+2*J] = product2[j][i];
//            degMat[i][j+3*J] = product3[j][i];
//        }
//    }
/*
	cout << "Printing the singular values of the A matrix" << endl;
	for(int i = 0; i < 1; i++){
		for(int j = 0; j < n; j++){
			cout << s[i+j] << " , ";
		}
	}
	//M = 6, n = 5 and a = mxn, lda = n, ldu = m, ldvt = n
	//J = 10, K = 5 and A = JxK
	cout << "\n" << endl;
	cout << "Printing the Left Singular Vectors (stored Column wise)" << endl;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < K; j++){
			cout << u[i*J+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	cout << "Printing the Right singular vectors (stored rowwise)" << endl;
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << vt[i*K+j] << " , ";
		}
		cout << "\n";
	}
	cout << "\n" << endl;
	*/
//copying over the U Matrices into the degMat container
		for(int i = 0; i < J; i++){
			for(int j = 0; j < J; j++){
				// The 4 lines of code below are incorrect and were corrected later on 17/01/2018
				/*
				//Central Stencil
				degMat[i][j] = u[i+j*lda];
				//Directional Stencil 1
				degMat[i][j+3*K] = u1[i+j*lda];
				//Directional stencil 2
				degMat[i][j+6*K] = u2[i+j+lda];
				//Directional Stencil 3
				degMat[i][j+9*K] = u3[i+j+lda];
				*/
				// The code set below to copy the U matrix is correct and was added on 17/01/2018
				//Central Stencil
				// 07/03/2018 K in the j iteration changed to J
				degMat[i][j] = u[i*J+j];
				//Directional Stencil 1
				degMat[i][j+2*J] = u1[i*J+j];
				//Directional stencil 2
				degMat[i][j+4*J] = u2[i*J+j];
				//Directional Stencil 3
				degMat[i][j+6*J] = u3[i*J+j];
			}
		}
		
//copying over the W diagonal singular value matrix into the degMat container
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			/*
			//Central Stencil Singular Values and the VT
			degMat[i][j+K] = s[i]; degMat[i][j+2*K] = vt[i+j*lda];
			// Directional Stencil 1
			degMat[i][j+4*K] = s1[i]; degMat[i][j+5*K] = vt1[i+j*lda];
			// Directional Stencil 2
			degMat[i][j+7*K] = s2[i]; degMat[i][j+8*K] = vt2[i+j*lda];
			// Directional Stencil 3
			degMat[i][j+10*K] = s3[i]; degMat[i][j+11*K] = vt3[i+j*lda];
			*/
			// code for the singular values 
			
			// 07/03/2018 CORRECTION MADE TO THE INDICES OF STORAGE initially K, 4K, 7K, 10K
			// CHANGED TO 2K, 5K, 8K, 11K
			if(i == j){
				// Central Stencil
				degMat[i][j+2*K] = s[i];
				// Directional Stencil 1
				degMat[i][j+6*K] = s1[i];
				// Directional Stencil 2
				degMat[i][j+10*K] = s2[i];
				// Directional Stencil 3
				degMat[i][j+14*K] = s3[i];
			}else{
				// Central Stencil
				degMat[i][j+2*K] = 0.0;
				// Directional Stencil 1
				degMat[i][j+6*K] = 0.0;
				// Directional Stencil 2
				degMat[i][j+10*K] = 0.0;
				// Directional Stencil 3
				degMat[i][j+14*K] = 0.0;
			}
			// Code added on the 17/01/2018 verified as correct
			//Central Stencil Singular Values and the VT
			degMat[i][j+3*K] = vt[i*K+j];
			// Directional Stencil 1
			degMat[i][j+7*K] = vt1[i*K+j];
			// Directional Stencil 2
			degMat[i][j+11*K] = vt2[i*K+j];
			// Directional Stencil 3
			degMat[i][j+15*K] = vt3[i*K+j];
		}
	}
	/*
	cout << "Outputting the singular values of the Aij matrix" << endl;
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << degMat[i][j+2*K] << " , ";
		}
		cout << "\n";
	}
	cout << "Outputting the V transpose matrix" << endl;
	for(int i = 0; i < K; i++){
		for(int j = 0; j < K; j++){
			cout << degMat[i][j+3*K] << " , ";
		}
		cout << "\n";
	}
	
	//DEG MAT WORKS AS INTENDED
	cout << "------------------------------------" << endl;
	cout << "Printing the Matrix Container degMat" << endl;
	cout << "------------------------------------" << endl;
	for(int i = 0; i < J; i++){
		for(int j = 0; j < 4*K; j++){
			cout << degMat[i][j] << " , ";
		}
		cout << "\n" << endl;
	}
	exit(0);
	*/
// copying the Vt values into the degMat container
//	exit(0);
   return degMat;
}								

su2double** CPhysicalGeometry::TriGaussPoints(int n){
    // this functions)
    su2double** xw = 0;
    //xw = (float&)malloc(n*sizeof(float));
    //(float*)malloc(n*sizeof(float));
    //xw = new su2double*[n+1]; N = 3
    xw = new su2double*[n+2];
    for(int h = 0; h < (n+2); h++){
        xw[h] = new su2double[3];
    }
	
    if(n == 1){
        xw[0][0] = 1.0/3.0; xw[0][1] = 1.0/3.0; xw[0][2] = 1.0;
    }else if(n == 2){
        xw[0][0] = 1.0/6.0; xw[0][1] = 1.0/6.0; xw[0][2] = 1.0/3.0;
        xw[1][0] = 2.0/3.0; xw[1][1] = 1.0/6.0; xw[1][2] = 1.0/3.0;
        xw[2][0] = 1.0/6.0; xw[2][1] = 2.0/3.0; xw[2][2] = 1.0/3.0;
    }else if(n == 3){
        xw[0][0] = 1.0/3.0; xw[0][1] = 1.0/3.0; xw[0][2] = -27.0/48.0;
        xw[1][0] = 0.2; xw[1][1] = 0.2; xw[1][2] = 25.0/48.0;
        xw[2][0] = 0.2; xw[2][1] = 0.6; xw[2][2] = 25.0/48.0;
        xw[3][0] = 0.6; xw[3][1] = 0.2; xw[3][2] = 25.0/48.0;
    }else if(n == 4){
        xw[0][0] = 0.44594849091597; xw[0][1] = 0.44594849091597; xw[0][2] = 0.22338158967801;
        xw[1][0] = 0.44594849091597; xw[1][1] = 0.10810301816807; xw[1][2] = 0.22338158967801;      
		xw[2][0] = 0.10810301816807; xw[2][1] = 0.44594849091597; xw[2][2] = 0.22338158967801;
        xw[3][0] = 0.09157621350977; xw[3][1] = 0.09157621350977; xw[3][2] = 0.10995174365532;
        xw[4][0] = 0.09157621350977; xw[4][1] = 0.81684757298046; xw[4][2] = 0.10995174365532;
		xw[5][0] = 0.81684757298046; xw[5][1] = 0.09157621350977; xw[5][2] = 0.10995174365532;
    }else if(n == 5){
        xw[0][0] = 1.0/3.0; xw[0][1] = 1.0/3.0; xw[0][2] = 0.225;
        xw[1][0] = 0.47014206410511; xw[1][1] = 0.47014206410511; xw[1][2] = 0.13239415278851;
        xw[2][0] = 0.47014206410511; xw[2][1] = 0.05971587178977; xw[2][2] = 0.13239415278851;
        xw[3][0] = 0.05971587178977; xw[3][1] = 0.47014206410511; xw[3][2] = 0.13239415278851;
        xw[4][0] = 0.10128650732346; xw[4][1] = 0.10128650732346; xw[4][2] = 0.12593918054483;
        xw[5][0] = 0.10128650732346; xw[5][1] = 0.79742698535309; xw[5][2] = 0.12593918054483;
		xw[6][0] = 0.79742698535309; xw[6][1] = 0.10128650732346; xw[6][2] = 0.12593918054483; 
    }  
    return xw;
}
void CPhysicalGeometry::SetOscillatorMatrix(void){

	bool weno_3 = true;
	bool weno_5 = false; 
	bool weno_7 = false;
	bool GEN_ORTHOG = true;
	bool SHIFTED_LEGENDRE = false;
	bool REGULAR_LEGENDRE = false;
	
	int K;
	if(weno_3){
		K = 5;
	}else if(weno_5){
		K = 14;
 	}
	//su2double oscillator_matrix[K][K]; 
	
	//su2double** oscillator_matrix = 0;
	oscillator_matrix = new su2double*[K];
	for(int i = 0; i < K; i++){
		oscillator_matrix[i] = new su2double[K]; 
		for(int j = 0; j < K; j++){
			oscillator_matrix[i][j] = 0.0;
		}
	}
	
	if(weno_3){
		if(GEN_ORTHOG){
			cout << "Setting the Oscillator Indicator Matrix for Generic Orthogonal Polynomials" << endl;
			oscillator_matrix[0][0] = 0.5; oscillator_matrix[0][1] = 0.0; oscillator_matrix[0][2] = 1.0/3.0; oscillator_matrix[0][3] = 1.0/6.0; oscillator_matrix[0][4] = 0.0;
			oscillator_matrix[1][0] = 0.0; oscillator_matrix[1][1] = 0.5; oscillator_matrix[1][2] = 0.0; oscillator_matrix[1][3] = 1.0/6.0; oscillator_matrix[1][4] = 1.0/3.0;
			oscillator_matrix[2][0] = 1.0/3.0; oscillator_matrix[2][1] = 0.0; oscillator_matrix[2][2] = 7.0/3.0; oscillator_matrix[2][3] = 1.0/12.0; oscillator_matrix[2][4] = 0.0;
			oscillator_matrix[3][0] = 1.0/6.0; oscillator_matrix[3][1] = 1.0/6.0; oscillator_matrix[3][2] = 1.0/12.0; oscillator_matrix[3][3] = 2.0/3.0; oscillator_matrix[3][4] = 1.0/12.0; 
			oscillator_matrix[4][0] = 0.0; oscillator_matrix[4][1] = 1.0/3.0; oscillator_matrix[4][2] = 0.0; oscillator_matrix[4][3] = 1.0/12.0; oscillator_matrix[4][4] = 7.0/3.0;
		}
		if(SHIFTED_LEGENDRE){
			cout << "Setting the Oscillator Indicator Matrix for Shifted Legendre Polynomials" << endl;
			oscillator_matrix[0][0] = 2.0; oscillator_matrix[0][1] = 0.0; oscillator_matrix[0][2] = -2.0; oscillator_matrix[0][3] = -2.0/3.0; oscillator_matrix[0][4] = 0.0;
			oscillator_matrix[1][0] = 0.0; oscillator_matrix[1][1] = 2.0; oscillator_matrix[1][2] = 0.0; oscillator_matrix[1][3] = -2.0/3.0; oscillator_matrix[1][4] = -2.0;
			oscillator_matrix[2][0] = -2.0; oscillator_matrix[2][1] = 0.0; oscillator_matrix[2][2] = 78.0; oscillator_matrix[2][3] = 0.0; oscillator_matrix[2][4] = 0.0;
			oscillator_matrix[3][0] = -2.0/3.0; oscillator_matrix[3][1] = -2.0/3.0; oscillator_matrix[3][2] = 0.0; oscillator_matrix[3][3] = 28.0/3.0; oscillator_matrix[3][4] =0.0; 
			oscillator_matrix[4][0] = 0.0; oscillator_matrix[4][1] = -2.0; oscillator_matrix[4][2] = 0.0; oscillator_matrix[4][3] = 0.0; oscillator_matrix[4][4] = 78.0;
		}
		if(REGULAR_LEGENDRE){
			cout << "Setting the Oscillator Indicator Matrix for Regular Legendre Polynomials" << endl;
			oscillator_matrix[0][0] = 0.5; oscillator_matrix[0][1] = 0.0; oscillator_matrix[0][2] = 0.5; oscillator_matrix[0][3] = 1.0/6.0; oscillator_matrix[0][4] = 0.0;
			oscillator_matrix[1][0] = 0.0; oscillator_matrix[1][1] = 0.5; oscillator_matrix[1][2] = 0.0; oscillator_matrix[1][3] = 1.0/6.0; oscillator_matrix[1][4] = 0.5;
			oscillator_matrix[2][0] = 0.5; oscillator_matrix[2][1] = 0.0; oscillator_matrix[2][2] = 63.0/12.0; oscillator_matrix[2][3] = 1.0/8.0; oscillator_matrix[2][4] = 0.0;
			oscillator_matrix[3][0] = 1.0/6.0; oscillator_matrix[3][1] = 1.0/6.0; oscillator_matrix[3][2] = 1.0/8.0; oscillator_matrix[3][3] = 2.0/3.0; oscillator_matrix[3][4] = 1.0/8.0; 
			oscillator_matrix[4][0] = 0.0; oscillator_matrix[4][1] = 0.5; oscillator_matrix[4][2] = 0.0; oscillator_matrix[4][3] = 1.0/8.0; oscillator_matrix[4][4] = 63.0/12.0;
		}
	}
	//return oscillator_matrix;
}

void CPhysicalGeometry::SetDegGlobalMatrix(void){
//	cout << "Calculating the cell averages of basis functions Matrix for WENO scheme" << endl;
    int J = 10, K = 5, elemCount = 0; // numStencil = 6844;
    //int numStencil = 16992;
    int numStencil = SetWenoNumCells();
   // unsigned long** stencil_weno = GetWeno_Stencil(); 
	//unsigned long** stencil_weno = 0; New INIT
	stencil_weno = GetWeno_Stencil();
	//stencil_weno[5][20] = 266; stencil_weno[8][20] = 270; stencil_weno[8][21] = 270;
	unsigned long stencil[4*(J+1)], stencil_nodes[3][4*J];
	
    //Initialising the wenorCoord matrix
    //   stencil_weno = (unsigned long**)malloc(weno_cells*sizeof(unsigned long*));
    //for(int i = 0; i < weno_cells; i++){
    //    stencil_weno[i] = (unsigned long*)malloc((4*(M+1))*sizeof(unsigned long));  
    //    for(int j = 0; j < 4*(M+1); j++){
    //        stencil_weno[i][j] = 0;
    //    }
    //}
    //unsigned long wenorCoord[numStencil*3][4*J]; //OLD INIT
    //unsigned long** wenorCoord;                     // NEW INIT
    //cout << "initing the wenorCoord" << endl;
    //wenorCoord = (unsigned long**)malloc((3*numStencil)*sizeof(unsigned long*));
    //for(int i = 0; i < 3*numStencil; i++){
    //    wenorCoord[i] = (unsigned long*)malloc((4*J)*sizeof(unsigned long));
        //for(int j = 0; 4*J; j++){
        //    wenorCoord[i][j] = 0; 
        //}
    //}
    //cout << "after initing the wenorCoord" << endl;
	// NEW INIT
	/*
    su2double*** p3DArray; 
    // numStencilxKx4*J
    p3DArray = (su2double***)malloc(numStencil*sizeof(su2double**));
    for(int i = 0; i < numStencil; i++){
        p3DArray[i] = (su2double**)malloc(K*sizeof(su2double*));
        for(int j = 0; j < K; j++){
            p3DArray[i][j] = (su2double*)malloc((4*J)*sizeof(su2double));
            for(int k = 0; k < 4*J; k++){
                p3DArray[i][j][k] = 0.0;
            }
        }
    }
    */
	su2double** xrCoord = 0;
	su2double** yrCoord = 0;
	//stencil_weno = new unsigned long*[numStencil];
	//for(int i = 0; i < numStencil; i++){
	//	stencil_weno[i] = new unsigned long[4*(J+1)];
	//	for(int j = 0; j < 4*(J+1); j++){
	//		stencil_weno[i][j] = 0;
	//	}
	//}
	//cout << "copying over the stencil data" << endl;
	
	//cout << "Finished copying over stencil data" << endl;
	//Allocating the memory for the 3D Array which will be used to store the stencil data
	//su2double*** p3DArray = 0;
    // OLD p3DARRAY INITIALISATION
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
    
	p3DArray = new su2double**[numStencil];
//	cout << "Initialisation of the p3DArray" << endl;
	/*
	for(int i = 0; i < numStencil; i++){
		p3DArray[i] = new su2double*[K];
		for(int j = 0; j < K; j++){
			p3DArray[i][j] = new su2double[4*J];
			for(int k = 0; k < 4*J; k++){
				p3DArray[i][j][k] = 0.0;
			}
		}
	}
    */
	for(int i = 0; i < numStencil; i++){
		p3DArray[i] = new su2double*[J];
		for(int j = 0; j < J; j++){
			p3DArray[i][j] = new su2double[16*K]; //07/03/2018 12K changed to 16K
			for(int k = 0; k < 16*K; k++){
				p3DArray[i][j][k] = 0.0; 
			}
		}
	}
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
//	cout << "Initialisation of the Reference Coordinate Containers" << endl;
	xrCoord = new su2double*[4*J]; yrCoord = new su2double*[4*J];
	for(int i = 0; i < 4*J; i++){
		xrCoord[i] = new su2double[3];
		yrCoord[i] = new su2double[3];
		for(int j = 0; j < 3; j++){
			xrCoord[i][j] = 0.0; yrCoord[i][j] = 0.0;
		}
	}
	
	//Getting the nodes 
//	cout << "Iterating over the elements to obtain the nodes" << endl;
	for(elemCount = 0; elemCount < numStencil; elemCount++){
		for(int i = 0; i < 4*(J+1); i++){
			stencil[i] = stencil_weno[elemCount][i];
			//PATCH
			//if(stencil[i] > 10000){
			//	stencil[i] = stencil[i-1];
			//}
			//END PATCH FOR BAD STENCIL 
		//	cout << stencil[i] << " , ";
		}
//		cout << "Stencil Weno[6][21] = " << stencil_weno[5][20] << endl;
//		cout << "Stencil Transfer successful, iterate = " << elemCount << endl;
		//cout << "\n";
		//-------------------------------------------------------------------------------------------//
		//-----------------------TRANSFORMING TO REFERENCE COORDINATE SPACE--------------------------//
		//-------------------------------------------------------------------------------------------//
		
		// TAKING IN NODES OF MASTER ELEMENT
		unsigned long nodeM1, nodeM2, nodeM3;
		su2double x1M, y1M, x2M, y2M, x3M, y3M;
		su2double wenoJacobian[2][2], wenodetJ, iwenodetJ, inversewenoJacobian[2][2];
		unsigned long node1, node2, node3, node4, node5, node6, node7, node8, node9, node10, node11, node12;
		
		nodeM1 = elem[stencil[0]]->GetNode(0); nodeM2 = elem[stencil[0]]->GetNode(1); nodeM3 = elem[stencil[0]]->GetNode(2);
		
//		cout << "PRINTING INFORMATION IN THE MASTER FUNCTION WEOW" << endl;
//		for(int i = 0; i < 8; i++){
//			for(int j = 0; j < 4*J; j++){
//				cout << stencil_weno[i][j] << " , ";
//			}
//			cout << "\n";
//		}
		//initialising the Jacobian and Inverse Jacobian matrices for the coordinate tranformation
//		cout << "Calculating the Jacobian and its inverse " << endl;
		for(int i = 0; i < 2; i++){
			for(int j = 0; j < 2; j++){
				wenoJacobian[i][j] = 0.0;
				inversewenoJacobian[i][j] = 0.0;
			}
		}
		
		x1M = node[nodeM1]->GetCoord(0); y1M = node[nodeM1]->GetCoord(1);
		x2M = node[nodeM2]->GetCoord(0); y2M = node[nodeM2]->GetCoord(1);
		x3M = node[nodeM3]->GetCoord(0); y3M = node[nodeM3]->GetCoord(1);
		
		//some code to find the node which connects the two sides of the triangle with same length
		/*
		su2double dist1, dist2, dist3;
		su2double mx1, mx2, mx3, my1, my2, my3;
		dist1 = sqrt((x1M - x2M)*(x1M - x2M) + (y1M - y2M)*(y1M - y2M)); 
		dist2 = sqrt((x1M - x3M)*(x1M - x3M) + (y1M - y3M)*(y1M - y3M)); 
		dist3 = sqrt((x2M - x3M)*(x2M - x3M) + (y2M - y3M)*(y2M - y3M));
		
		if(dist1 == dist2){
			//Node 1 is the node connecting the two short lengths
			mx1 = x1M; my1 = y1M; mx2 = x2M; my2 = y2M; mx3 = x3M; my3 = y3M;
		}else if(dist1 == dist3){
			// Node 2 is the node connecting the two short lengths
			mx1 = x2M; my1 = y2M; mx2 = x1M; my2 = y1M; mx3 = x3M; my3 = y3M;
		}else{
			// Node 3 is the node connecting the two short lengths
			mx1 = x3M; my1 = y3M; mx2 = x1M; my2 = y1M; mx3 = x2M; my3 = y2M;
		}
		
		// reallocating to x1M etc
		x1M = mx1; x2M = mx2; x3M = mx3;
		y1M = my1; y2M = my2; y3M = my3;
		*/
//		cout << "x1M = " << x1M << " y1M = " << y1M << endl;
//		cout << "x2M = " << x2M << " y2M = " << y2M << endl;
//		cout << "x3M = " << x3M << " y3M = " << y3M << endl;

		wenoJacobian[0][0] = x2M - x1M; wenoJacobian[0][1] = x3M - x1M;
		wenoJacobian[1][0] = y2M - y1M; wenoJacobian[1][1] = y3M - y1M;
		
//		wenoJacobian[0][0] = node[nodeM2]->GetCoord(0) - node[nodeM1]->GetCoord(0); wenoJacobian[0][1] = node[nodeM3]->GetCoord(0) - node[nodeM1]->GetCoord(0);
//		wenoJacobian[1][0] = node[nodeM2]->GetCoord(1) - node[nodeM1]->GetCoord(1); wenoJacobian[1][1] = node[nodeM3]->GetCoord(1) - node[nodeM1]->GetCoord(1);
		
//		cout << "Printing wenoJacobian" << endl;
//		for(int i = 0; i <exit(0); 2; i++){
//			for(int j = 0; j < 2; j++){
//				cout << wenoJacobian[i][j] << " , ";
//			}
//			cout << "\n";
//		}
		wenodetJ = (wenoJacobian[0][0]*wenoJacobian[1][1] - wenoJacobian[0][1]*wenoJacobian[1][0]); // was wenoJacobian[0][0]*wenoJacobian[1][1] - wenoJacobian[1][0]*wenoJacobian[0][1]
		iwenodetJ = 1.0/(wenodetJ+1.0E-06); //10.0E-16
//		if(wenodetJ == 0.0){
//			//PRE PATCH OCTOBER 12th THIS WAS UNCOMMENTED
//			//iwenodetJ = 1.0;
//			inversewenoJacobian[0][0] = 0.0;
//			inversewenoJacobian[0][1] = 0.0;
//			inversewenoJacobian[1][0] = 0.0;
//			inversewenoJacobian[1][1] = 0.0;
//		}else{
//			iwenodetJ = 1.0/wenodetJ;
//			inversewenoJacobian[0][0] = 1.0*iwenodetJ*wenoJacobian[1][1];
//			inversewenoJacobian[0][1] = -1.0*iwenodetJ*wenoJacobian[0][1];
//			inversewenoJacobian[1][0] = -1.0*iwenodetJ*wenoJacobian[1][0];
//			inversewenoJacobian[1][1] = 1.0*iwenodetJ*wenoJacobian[0][0];
//		}
		
		//PRE PATCH 12th OCTOBER THIS WORKED
		inversewenoJacobian[0][0] = 1.0*iwenodetJ*wenoJacobian[1][1];
		inversewenoJacobian[0][1] = -1.0*iwenodetJ*wenoJacobian[0][1];
		inversewenoJacobian[1][0] = -1.0*iwenodetJ*wenoJacobian[1][0];
		inversewenoJacobian[1][1] = 1.0*iwenodetJ*wenoJacobian[0][0];
		su2double testx1, testy1, testx2, testy2, testx3, testy3;
		testx1 = inversewenoJacobian[0][0]*(node[nodeM1]->GetCoord(0)-x1M) + inversewenoJacobian[0][1]*(node[nodeM1]->GetCoord(1)-y1M);
		testy1 = inversewenoJacobian[1][0]*(node[nodeM1]->GetCoord(0)-x1M) + inversewenoJacobian[1][1]*(node[nodeM1]->GetCoord(1)-y1M);
		testx2 = inversewenoJacobian[0][0]*(node[nodeM2]->GetCoord(0)-x1M) + inversewenoJacobian[0][1]*(node[nodeM2]->GetCoord(1)-y1M);
		testy2 = inversewenoJacobian[1][0]*(node[nodeM2]->GetCoord(0)-x1M) + inversewenoJacobian[1][1]*(node[nodeM2]->GetCoord(1)-y1M);
		testx3 = inversewenoJacobian[0][0]*(node[nodeM3]->GetCoord(0)-x1M) + inversewenoJacobian[0][1]*(node[nodeM3]->GetCoord(1)-y1M);
		testy3 = inversewenoJacobian[1][0]*(node[nodeM3]->GetCoord(0)-x1M) + inversewenoJacobian[1][1]*(node[nodeM3]->GetCoord(1)-y1M);
	
//		for(int i = 0; i < 2; i++){
//			for(int j = 0; j < 2; j++){
//				cout << inversewenoJacobian[i][j] << " , ";
//			}
//			cout << "\n";
//		}
//		cout << "Jacobian matrix calculations successful" << endl; 
//		exit(0);
		su2double rCoord[6][4*J], rCoordc[6][J], rCoord1[6][J], rCoord2[6][J], rCoord3[6][J];
		su2double deltax1, deltax2, deltax3, deltay1, deltay2, deltay3;
		su2double phi[6][4*J];
		for(int i = 0; i < 6; i++){
			for(int j = 0; j < 4*J; j++){
				rCoord[i][j] = 0.0;
			}
		}
		// WHAT IS THIS ? 16/01/2018
		//inversewenoJacobian[0][0] = 1;
		//inversewenoJacobian[0][1] = 1;
		//inversewenoJacobian[1][1] = 1;
		//inversewenoJacobian[1][0] = 1;
		// WHAT IS THIS ? ^
//		cout << "Nodal conversion of coordinates from reference to physical" << endl;
		for(int i = 0; i < J; i++){
//			cout << "Defining the nodes, iterate = " << i << endl;
			node1 = elem[stencil[i+1]]->GetNode(0); node2 = elem[stencil[i+1]]->GetNode(1); node3 = elem[stencil[i+1]]->GetNode(2);													//central stencil coordinates
//			cout << "Central Stencil Nodal Allocation done" << endl;
//			cout << "ElemCount = " << elemCount << " Stencil Container = " << stencil[i+J+1] << endl;
			node4 = elem[stencil[i+J+1]]->GetNode(0); node5 = elem[stencil[i+J+1]]->GetNode(1); node6 = elem[stencil[i+J+1]]->GetNode(2);											//face 1 directional stencil coordinates
//			cout << "Directional Stencil 1 Nodal Allocation done" << endl;
			node7 = elem[stencil[i+2*(J+1)]]->GetNode(0); node8 = elem[stencil[i+2*(J+1)]]->GetNode(1); node9 = elem[stencil[i+2*(J+1)]]->GetNode(2);								//face 2 directional stencil coordinates 
//			cout << "Directional Stencil 2 Nodal Allocation done " << endl;
			node10 = elem[stencil[i+3*(J+1)]]->GetNode(0); node11 = elem[stencil[i+3*(J+1)]]->GetNode(1); node12 = elem[stencil[i+3*(J+1)]]->GetNode(2);							//face 3 directional stencil coordinates
//			cout << "Node determination successful, iterate = " << i << endl;
			deltax1 = node[node1]->GetCoord(0) - x1M; deltax2 = node[node2]->GetCoord(0) - x1M; deltax3 = node[node3]->GetCoord(0) - x1M;	
			deltay1 = node[node1]->GetCoord(1) - y1M; deltay2 = node[node2]->GetCoord(1) - y1M; deltay3 = node[node3]->GetCoord(1) - y1M;
			
			rCoordc[0][i] = inversewenoJacobian[0][0]*(deltax1) + inversewenoJacobian[0][1]*(deltay1);
			rCoordc[1][i] = inversewenoJacobian[1][0]*(deltax1) + inversewenoJacobian[1][1]*(deltay1);
			rCoordc[2][i] = inversewenoJacobian[0][0]*(deltax2) + inversewenoJacobian[0][1]*(deltay2);
			rCoordc[3][i] = inversewenoJacobian[1][0]*(deltax2) + inversewenoJacobian[1][1]*(deltay2);
			rCoordc[4][i] = inversewenoJacobian[0][0]*(deltax3) + inversewenoJacobian[0][1]*(deltay3);
			rCoordc[5][i] = inversewenoJacobian[1][0]*(deltax3) + inversewenoJacobian[1][1]*(deltay3);

			deltax1 = node[node4]->GetCoord(0) - x1M; deltax2 = node[node5]->GetCoord(0) - x1M; deltax3 = node[node6]->GetCoord(0) - x1M;
			deltay1 = node[node4]->GetCoord(1) - y1M; deltay2 = node[node5]->GetCoord(1) - y1M; deltay3 = node[node6]->GetCoord(1) - y1M;
			
			rCoord1[0][i] = inversewenoJacobian[0][0]*(deltax1) + inversewenoJacobian[0][1]*(deltay1);
			rCoord1[1][i] = inversewenoJacobian[1][0]*(deltax1) + inversewenoJacobian[1][1]*(deltay1);
			rCoord1[2][i] = inversewenoJacobian[0][0]*(deltax2) + inversewenoJacobian[0][1]*(deltay2);
			rCoord1[3][i] = inversewenoJacobian[1][0]*(deltax2) + inversewenoJacobian[1][1]*(deltay2);
			rCoord1[4][i] = inversewenoJacobian[0][0]*(deltax3) + inversewenoJacobian[0][1]*(deltay3);
			rCoord1[5][i] = inversewenoJacobian[1][0]*(deltax3) + inversewenoJacobian[1][1]*(deltay3);
			
			deltax1 = node[node7]->GetCoord(0) - x1M; deltax2 = node[node8]->GetCoord(0) - x1M; deltax3 = node[node9]->GetCoord(0) - x1M;
			deltay1 = node[node7]->GetCoord(1) - y1M; deltay2 = node[node8]->GetCoord(1) - y1M; deltay3 = node[node9]->GetCoord(1) - y1M;
			
			rCoord2[0][i] = inversewenoJacobian[0][0]*(deltax1) + inversewenoJacobian[0][1]*(deltay1);
			rCoord2[1][i] = inversewenoJacobian[1][0]*(deltax1) + inversewenoJacobian[1][1]*(deltay1);
			rCoord2[2][i] = inversewenoJacobian[0][0]*(deltax2) + inversewenoJacobian[0][1]*(deltay2);
			rCoord2[3][i] = inversewenoJacobian[1][0]*(deltax2) + inversewenoJacobian[1][1]*(deltay2);
			rCoord2[4][i] = inversewenoJacobian[0][0]*(deltax3) + inversewenoJacobian[0][1]*(deltay3);
			rCoord2[5][i] = inversewenoJacobian[1][0]*(deltax3) + inversewenoJacobian[1][1]*(deltay3);

			deltax1 = node[node10]->GetCoord(0) - x1M; deltax2 = node[node11]->GetCoord(0) - x1M; deltax3 = node[node12]->GetCoord(0) - x1M;
			deltay1 = node[node10]->GetCoord(1) - y1M; deltay2 = node[node11]->GetCoord(1) - y1M; deltay3 = node[node12]->GetCoord(1) - y1M;
			
			rCoord3[0][i] = inversewenoJacobian[0][0]*(deltax1) + inversewenoJacobian[0][1]*(deltay1);
			rCoord3[1][i] = inversewenoJacobian[1][0]*(deltax1) + inversewenoJacobian[1][1]*(deltay1);
			rCoord3[2][i] = inversewenoJacobian[0][0]*(deltax2) + inversewenoJacobian[0][1]*(deltay2);
			rCoord3[3][i] = inversewenoJacobian[1][0]*(deltax2) + inversewenoJacobian[1][1]*(deltay2);
			rCoord3[4][i] = inversewenoJacobian[0][0]*(deltax3) + inversewenoJacobian[0][1]*(deltay3);
			rCoord3[5][i] = inversewenoJacobian[1][0]*(deltax3) + inversewenoJacobian[1][1]*(deltay3);
		}
//		cout << "Transferring the reference coordinate data to one global matrix" << endl;
		for(int i = 0; i < 6; i++){
			for(int j = 0; j < J; j++){
				rCoord[i][j] = rCoordc[i][j]; rCoord[i][j+J] = rCoord1[i][j]; rCoord[i][j+2*J] = rCoord2[i][j]; rCoord[i][j+3*J] = rCoord3[i][j];
			}
		}
		/*
		cout << "Printing the Physical Coordinates" << endl;
		for(int i = 0; i < 6; i++){
			for(int j = 0; j < 4*J; j++){
				cout << pCoord[i][j] << " , ";
			}
			cout << "\n";
		}
		*/
		
		/*
		cout << "Printing the Reference Coordinates" << endl;
		for(int i = 0; i < 6; i++){
			for(int j = 0; j < 4*J; j++){
				cout << rCoord[i][j] << " , ";
			}
			cout << "\n";
		}
		*/
		for(int i = 0; i < 4*J; i++){
			xrCoord[i][0] = rCoord[0][i]; xrCoord[i][1] = rCoord[2][i]; xrCoord[i][2] = rCoord[4][i];
			yrCoord[i][0] = rCoord[1][i]; yrCoord[i][1] = rCoord[3][i]; yrCoord[i][2] = rCoord[5][i]; 
		}
//		cout << "---------------------------------------------" << endl;
//		cout << "---------- PRINTING X-REFERENCE--------------" << endl;
//		cout << "---------------------------------------------" << endl;
//		cout << "\n";

//		for(int i = 0; i < 4*J; i++){
//			for(int j = 0; j < 3; j++){
//				cout << xrCoord[i][j] << " , ";
//			}
//			cout << "\n";
//		}
//		
//		cout << "\n";
//		cout << "---------------------------------------------" << endl;
///		cout << "---------- PRINTING Y-REFERENCE--------------" << endl;
//		cout << "---------------------------------------------" << endl;
//		cout << "\n";
		
//		for(int i = 0; i < 4*J; i++){
//			for(int j = 0; j < 3; j++){
//				cout << yrCoord[i][j] << " , ";
//			}
//			cout << "\n";
//		}	
		
		//unsigned long** A = SetDegMatrix(J, K, 3, xrCoord, yrCoord);
		
		//su2double** A = SetDegMatrix(10, 5, 3, xrCoord, yrCoord);
		//su2double** A = SetDegMatrix(J, K, 3, xrCoord, yrCoord); //original integraion call
		su2double** A = SetDegMatrix(J, K, 3, xrCoord, yrCoord);
		//inserting the data from A into p3DArray which will contain all the relevant data for the (A(T)*A)^(-1)*A(T) calculation for all the stencils for every element
		// in a 3D array
//		cout << "Storing the data in the p3dArray" << endl;
//		cout << "Iterate counter = " << elemCount << endl;

		//OLD PRE LAPACKE SVD DECOMPOSITION P3DARRAY ALLOCATION
//		for(int i = 0; i < K; i++){
//			for(int j = 0; j < 4*J; j++){
//				p3DArray[elemCount][i][j] = A[i][j];
//			}
//		}

		//NEW POST LAPACKE SVD DECOMPOSTION P3DARRAY ALLOCATION
		// 07/03/2018 12K changed to 16K
		for(int i = 0; i < J; i++){
			for(int j = 0; j < 16*K; j++){
				p3DArray[elemCount][i][j] = A[i][j];
			}
		}
//		cout << "\n";
//		cout << "-----------------------------------------------" << endl;
//		cout << "--------- PRINTING p3DArray MATRIX ------------" << endl;
//		cout << "-----------------------------------------------" << endl;
//		cout << "\n";
//		for(int i = 0; i < K; i++){
//			for(int j = 0; j < 4*J; j++){
//				cout << "A[" << i << "][" << j << "] = " << A[i][j] << endl;
//				cout << "p3DArray[" << elemCount << "][" << i << "][" << j << "] = " << p3DArray[elemCount][i][j] << endl;
//			}
//			cout << "\n" << endl;
//		}
		
	}
//	exit(0);
//	cout << "elemCount = " << elemCount << endl;
//	cout << "K = " << K << endl;
//	cout << "4*J = " << 4*J << endl;
//	for(int i = 0; i < elemCount; i++){
//		for(int j = 0; j < J; j++){
//			for(int k = 0; k < 12*K; k++){
//				cout << "iElem = " << i << " Value = " << p3DArray[i][j][k] << endl;
//			}
//			cout << "\n";
//		}
//		cout << "\n";
//	}
//	cout << "Successfully completed calculation of the Deg Global Matrix" << endl;
//	return p3DArray;
//	cout << "Array returned " << endl;
}

void CPhysicalGeometry::SetWeno_NodalContainer(void){
	//This functions takes in the
	//int weno_cells = 6844;
    //int weno_cells = 16992;
    int weno_cells = SetWenoNumCells();
    int K = 5, M = 2*K;
	unsigned long** stencil_weno = GetWeno_Stencil();
	unsigned long element; 
	vector<unsigned long> myvector; 
	vector<unsigned long>::iterator it; 
	set<unsigned long> myset;
	set<unsigned long>::iterator it2;
	unsigned long testNode;
	nodalContainer = new unsigned long**[weno_cells];
	for(int i = 0; i < weno_cells; i++){
		nodalContainer[i] = new unsigned long*[4*(M+1)];
		for(int j = 0; j < 4*(M+1); j++){
			nodalContainer[i][j] = new unsigned long[3];
			for(int k = 0; k < 3; k++){
				nodalContainer[i][j][k] = 0.0; 
			}
		}
	}
	//unsigned long nodalCotainer[weno_cells][4*(J+1)][3];
	for(int i = 0; i < weno_cells; i++){ // i < 8p
		for(int j = 0; j < 4*(M+1); j++){
			element = stencil_weno[i][j];
//			if(element > 10000){
//				element = stencil_weno[i][j-1];
//			}
			for(int k = 0; k < 3; k++){
				//cout << "element = " << element << endl;
				//exit(0);
				nodalContainer[i][j][k] = elem[element]->GetNode(k);
			}
		}
	}

	//searching the central stencil for unique nodes
	/*
	unsigned long uniqueCentral[M+3];
	for(int i = 0; i < M+3; i++){
		uniqueCentral[i] = 0;
	}
	int uniqueCount = 0;
	int testCheck = 0;
	for(int i = 0; i < (M+1); i++){
		for(int j = 0; j < 3; j++){
			testNode = nodalContainer[0][i][j];
			for(int ii = 0; ii < (M+1); ii++){
				for(int jj = 0; jj < 3; jj++){
					if(nodalContainer[0][ii][jj] == testNode){
						testCheck = 1;
					}
				}
			}
			if(testCheck == 0){
				uniqueCentral[uniqueCount] = testNode;
				uniqueCount++;
			}
			testCheck = 0;
		}
	}
	
	for(int i = 0; i < (M+1); i++){
		for(int j = 0; j < 3; j++){
			cout << nodalContainer[0][i][j] << " , ";
		}
		cout << "\n";
	}
	
	for(int i = 0; i < M+3; i++){
		cout << uniqueCentral[i] << " , " << endl;
	}
	
	exit(0);
	*/
	//testing some code to return only the unique nodes in unordered fashion per stencil
	/*
	for(int i = 0; i < M; i++){
		for(int j = 0; j < 3; j++){
			myset.insert(nodalContainer[0][i+1][j]);
		}
	}
	
	for(int i = 0; i < M; i++){
		for(int j = 0; j < 3; j++){
			for(it2=myset.begin(); it2 != myset.end(); ++it2){
				it = find(myvector.begin(), myvector.end(), *it2);
				if(it != myvector.end()){
					myvector.insert(it, *it);
				}
			}
		}		
	}
	
//	cout << "Central Stencil Contents" << endl;
//	for(int i = 0; i < M+1; i++){
//		cout << stencil_weno[0][i] << " , ";
//	}
//	cout << "\n" << endl;
//	cout << "Nodal Contaier Contents" << endl;
//	for(int i = 0; i < M+1; i++){
//		for(int j = 0; j < 3; j++){
//			cout << nodalContainer[0][i][j] << " , ";
//		}
//		cout << "\n";
//	}
	unsigned long test_container[M+3], uniqueNodeD1[M+3], uniqueNodeD2[M+3], uniqueNodeD3[M+3];
	for(int i = 0; i < M+3; i++){
		test_container[i] = 0; uniqueNodeD1[i] = 0; uniqueNodeD2[i] = 0; uniqueNodeD3[i] = 0; 
	}
	int testNodeCheck = 0, testNodeCheck2 = 0, testNodeCheck3 = 0, testNodeCheck4 = 0;
	int uniqueCount = 0, uniqueCountD1 = 0, uniqueCountD2 = 0, uniqueCountD3 = 0;
	int iterOffset = M+1;
	unsigned long testNode2, testNode3, testNode4;
	for(int ii = 0; ii < weno_cells; ii++){
		for(int i = 0; i < M+1; i++){
			for(int j = 0; j < 3; j++){
				testNode = nodalContainer[ii][i][j];
				testNode2 = nodalContainer[ii][i+iterOffset][j];
				testNode3 = nodalContainer[ii][i+2*iterOffset][j];
				testNode4 = nodalContainer[ii][i+3*iterOffset][j];
				for(int k = 0; k < M+3; k++){
					if(test_container[k] == testNode){		//checking central stencil
						testNodeCheck = 1;
					}
					if(uniqueNodeD1[k] == testNode2){
						testNodeCheck2 = 1;
					}
					if(uniqueNodeD2[k] == testNode3){
						testNodeCheck3 = 1;
					}
					if(uniqueNodeD3[k] == testNode4){
						testNodeCheck4 = 1;
					}
				}
				if(testNodeCheck == 0){
					test_container[uniqueCount] = testNode;
					uniqueCount += 1;
				}
				if(testNodeCheck2 == 0){
					uniqueNodeD1[uniqueCountD1] = testNode2;
					uniqueCountD1 += 1;
				}
				if(testNodeCheck3 == 0){
					uniqueNodeD2[uniqueCountD2] = testNode3;
					uniqueCountD2 += 1;
				}
				if(testNodeCheck4 == 0){
					uniqueNodeD3[uniqueCountD3] = testNode4;
					uniqueCountD3 += 1;
				}
				testNodeCheck = 0; testNodeCheck2 = 0; testNodeCheck3 = 0; testNodeCheck4 = 0;
			}
		}
		uniqueCount = 0; uniqueCountD1 = 0; uniqueCountD2 = 0; uniqueCountD3 = 0;
		for(int i = 0; i < M+3; i++){
			wenoNodes[ii][i] = test_container[i]; wenoNodes[ii][i+M+3] = uniqueNodeD1[i]; 
			wenoNodes[ii][i+2*(M+3)] = uniqueNodeD2[i]; wenoNodes[ii][i+3*(M+3)] = uniqueNodeD3[i];
		}
	}
	
	cout << "UniqueCount = " << uniqueCount << endl;
	cout << "UniqueCount D1 = " << uniqueCountD1 << endl;
	cout << "UniqueCount D2 = " << uniqueCountD2 << endl;
	cout << "UniqueCount D3 = " << uniqueCountD3 << endl;
	cout << "Outputting contents of Central Stencil Nodal Container" << endl;
	for(int i = 0; i < M+3; i++){
		cout << test_container[i] << " , ";
	}
	cout <<  "\n" << endl;	
	cout << "Outputting contents of Directional Stencil 1 Nodal Container" << endl;
	for(int i = 0; i < M+3; i++){
		cout << uniqueNodeD1[i] << " , ";
	}
	cout <<  "\n" << endl;
	cout << "Outputting contents of Directional Stencil 2 Nodal Container" << endl;
	for(int i = 0; i < M+3; i++){
		cout << uniqueNodeD2[i] << " , ";
	}
	cout <<  "\n" << endl;
	cout << "Outputting contents of Directional Stencil 3 Nodal Container" << endl;
	for(int i = 0; i < M+3; i++){
		cout << uniqueNodeD3[i] << " , ";
	}
	cout <<  "\n" << endl;
	cout << "Outputting the contents of Nodal Container" << endl;
	for(int i = 0; i < 4*iterOffset; i++){
		for(int j = 0; j < 3; j++){
			cout << nodalContainer[0][i][j] << " , ";
		}
		cout << "\n";
	}
	cout << "Unique WENO Nodes for all containers" << endl;
	for(int j = 0; j < weno_cells; j++){
		for(int i = 0; i < 4*(M+3); i++){
			cout << wenoNodes[j][i] << " , ";
		}
		cout << "\n"; 
	}
	cout << "\n" << endl;
	*/
//	cout << "Outputting the contents of the set" << endl;
//	for(it2 = myset.begin(); it2 != myset.end(); ++it2){
//		cout << " , " << *it2;
//	}
//	cout << "\n" << endl;
//	cout << "Outputt	ing the contents of testNodeCheck " << endl;
//	for(int i = 0; i < M+3; i++){
//		cout << test_container[i] << " , ";
//	}
	
	
//	cout << "\n" << endl;
//	cout << "Outputting the unique nodes for first weno_cell element's central stencil" << endl;
//	for(it = myvector.begin(); it < myvector.end(); it++){
//		cout << " " << *it;
//	}
	 
//	cout << "\n" << endl;
//	exit(0);
	
}

void CPhysicalGeometry::SetWeno_UniqueNodalContainer(void){
	int weno_cells = SetWenoNumCells();
	int K = 5, M = 10;
	unsigned long*** nodalContainer = GetWeno_NodalContainer();
	wenoNodes = new unsigned long*[weno_cells];
	for(int i = 0; i < weno_cells; i++){
		wenoNodes[i] = new unsigned long[4*(M+1)]; //OLD was M+3
		for(int j = 0; j < 4*(M+1); j++){
			wenoNodes[i][j] = 0;
		}
	}
	//OLD was M+3
	unsigned long test_container[M+1], uniqueNodeD1[M+1], uniqueNodeD2[M+1], uniqueNodeD3[M+1];

	int testNodeCheck = 0, testNodeCheck2 = 0, testNodeCheck3 = 0, testNodeCheck4 = 0;
	int uniqueCount = 0, uniqueCountD1 = 0, uniqueCountD2 = 0, uniqueCountD3 = 0;
	int iterOffset = M+1;
	unsigned long testNode, testNode2, testNode3, testNode4;
	for(int ii = 0; ii < weno_cells; ii++){
		for(int i = 0; i < M+3; i++){
			test_container[i] = 0; uniqueNodeD1[i] = 0; uniqueNodeD2[i] = 0; uniqueNodeD3[i] = 0; 
		}	
		for(int i = 0; i < M+1; i++){
			for(int j = 0; j < 3; j++){
				testNode = nodalContainer[ii][i][j];
				testNode2 = nodalContainer[ii][i+iterOffset][j];
				testNode3 = nodalContainer[ii][i+2*iterOffset][j];
				testNode4 = nodalContainer[ii][i+3*iterOffset][j];
				for(int k = 0; k < M+1; k++){//OLD was M+3
					if(test_container[k] == testNode){		//checking central stencil
						testNodeCheck = 1;
					}
					if(uniqueNodeD1[k] == testNode2){
						testNodeCheck2 = 1;
					}
					if(uniqueNodeD2[k] == testNode3){
						testNodeCheck3 = 1;
					}
					if(uniqueNodeD3[k] == testNode4){
						testNodeCheck4 = 1;
					}
				}

				if(testNodeCheck == 0){
					test_container[uniqueCount] = testNode;
					uniqueCount += 1;
				}
				if(testNodeCheck2 == 0){
					uniqueNodeD1[uniqueCountD1] = testNode2;
					uniqueCountD1 += 1;
				}
				if(testNodeCheck3 == 0){
					uniqueNodeD2[uniqueCountD2] = testNode3;
					uniqueCountD2 += 1;
				}
				if(testNodeCheck4 == 0){
					uniqueNodeD3[uniqueCountD3] = testNode4;
					uniqueCountD3 += 1;
				}
				testNodeCheck = 0; testNodeCheck2 = 0; testNodeCheck3 = 0; testNodeCheck4 = 0;
			}
		}

		uniqueCount = 0; uniqueCountD1 = 0; uniqueCountD2 = 0; uniqueCountD3 = 0;
		for(int i = 0; i < M+1; i++){//OLD was M+3
		//	if (ii == 7){
		//		cout << elem[73]->GetNode(0) << endl;
		//		cout << elem[73]->GetNode(1) << endl;
		//		cout << elem[73]->GetNode(2) << endl;
		//		cout << test_container[0] << endl;
		//		exit(0);
		//	}
			wenoNodes[ii][i] = test_container[i]; wenoNodes[ii][i+M+1] = uniqueNodeD1[i]; 
			wenoNodes[ii][i+2*(M+1)] = uniqueNodeD2[i]; wenoNodes[ii][i+3*(M+1)] = uniqueNodeD3[i];
		}
	}
	
	//FOR DEBUGGING ONLY
	/*
	cout << "Outputting the Stencil Nodes" << endl;
	for(int i = 0; i < M+1; i++){
		for(int j = 0; j < 3; j++){
			cout << nodalContainer[0][i][j] << " , ";
		}
		cout << "\n";
	} 
	
	cout << "\n" << endl;
	
	cout << "Outputting the unique nodes" << endl;
	for(int i = 0; i < weno_cells; i++){
		for(int j = 0; j < 4*(M+3); j++){
			cout << wenoNodes[i][j] << " , ";
		}
		cout << "\n";
	}
	exit(0);
	*/
}

void CPhysicalGeometry::SetBoundVolume(void) {
  unsigned short cont, iMarker, iElem, iNode_Domain, iNode_Surface;
  unsigned long Point_Domain, Point_Surface, Point, iElem_Surface, iElem_Domain;
  bool CheckVol;
  
  for (iMarker = 0; iMarker < nMarker; iMarker++)
    for (iElem_Surface = 0; iElem_Surface < nElem_Bound[iMarker]; iElem_Surface++) {
      
      /*--- Choose and arbitrary point from the surface --*/
      Point = bound[iMarker][iElem_Surface]->GetNode(0);
      CheckVol = false;
      
      for (iElem = 0; iElem < node[Point]->GetnElem(); iElem++) {
        /*--- Look for elements surronding that point --*/
        cont = 0; iElem_Domain = node[Point]->GetElem(iElem);
        for (iNode_Domain = 0; iNode_Domain < elem[iElem_Domain]->GetnNodes(); iNode_Domain++) {
          Point_Domain = elem[iElem_Domain]->GetNode(iNode_Domain);
          for (iNode_Surface = 0; iNode_Surface < bound[iMarker][iElem_Surface]->GetnNodes(); iNode_Surface++) {
            Point_Surface = bound[iMarker][iElem_Surface]->GetNode(iNode_Surface);
            if (Point_Surface == Point_Domain) cont++;
            if (cont == bound[iMarker][iElem_Surface]->GetnNodes()) break;
          }
          if (cont == bound[iMarker][iElem_Surface]->GetnNodes()) break;
        }
        
        if (cont == bound[iMarker][iElem_Surface]->GetnNodes()) {
          bound[iMarker][iElem_Surface]->SetDomainElement(iElem_Domain);
          CheckVol = true;
          break;
        }
      }
      if (!CheckVol) {
        cout << "The surface element ("<< iMarker <<", "<< iElem_Surface << ") doesn't have an associated volume element." << endl;
        exit(EXIT_FAILURE);
      }
    }
}

void CPhysicalGeometry::SetVertex(CConfig *config) {
  unsigned long  iPoint, iVertex, iElem;
  unsigned short iMarker, iNode;
  
  /*--- Initialize the Vertex vector for each node of the grid ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      node[iPoint]->SetVertex(-1, iMarker);
  
  /*--- Create and compute the vector with the number of vertex per marker ---*/
  
  nVertex = new unsigned long [nMarker];
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    
    /*--- Initialize the number of Bound Vertex for each Marker ---*/
    
    nVertex[iMarker] = 0;
    for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++)
      for (iNode = 0; iNode < bound[iMarker][iElem]->GetnNodes(); iNode++) {
        iPoint = bound[iMarker][iElem]->GetNode(iNode);
        
        /*--- Set the vertex in the node information ---*/
        
        if ((node[iPoint]->GetVertex(iMarker) == -1) || (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE)) {
          node[iPoint]->SetVertex(nVertex[iMarker], iMarker);
          nVertex[iMarker]++;
        }
      }
  }
  
  /*--- Initialize the Vertex vector for each node, the previous result is deleted ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      node[iPoint]->SetVertex(-1, iMarker);
  
  /*--- Create the bound vertex structure, note that the order
   is the same as in the input file, this is important for Send/Receive part ---*/
  
  vertex = new CVertex**[nMarker];
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    vertex[iMarker] = new CVertex* [nVertex[iMarker]];
    nVertex[iMarker] = 0;
    
    /*--- Initialize the number of Bound Vertex for each Marker ---*/
    
    for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++)
      for (iNode = 0; iNode < bound[iMarker][iElem]->GetnNodes(); iNode++) {
        iPoint = bound[iMarker][iElem]->GetNode(iNode);

        /*--- Set the vertex in the node information ---*/
        
        if ((node[iPoint]->GetVertex(iMarker) == -1) || (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE)) {
          iVertex = nVertex[iMarker];
          vertex[iMarker][iVertex] = new CVertex(iPoint, nDim);
          
          if (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) {
            vertex[iMarker][iVertex]->SetRotation_Type(bound[iMarker][iElem]->GetRotation_Type());
          }
          node[iPoint]->SetVertex(nVertex[iMarker], iMarker);
          nVertex[iMarker]++;
        }
      }
  }
}

void CPhysicalGeometry::ComputeNSpan(CConfig *config, unsigned short val_iZone, unsigned short marker_flag, bool allocate) {
  unsigned short iMarker, jMarker, iMarkerTP, iSpan, jSpan, kSpan = 0;
  unsigned long iPoint, iVertex;
  long jVertex;
  int nSpan, nSpan_loc;
  su2double *coord, *valueSpan, min, max, radius, delta;
  int rank = MASTER_NODE;
  short SendRecv;
  bool isPeriodic;
  unsigned short SpanWise_Kind = config->GetKind_SpanWise();
  
#ifdef HAVE_MPI
  unsigned short iSize;
  int size, nSpan_max;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  int My_nSpan, My_MaxnSpan, *My_nSpan_loc = NULL;
  su2double MyMin, MyMax, *MyTotValueSpan =NULL,*MyValueSpan =NULL;
#endif

  nSpan = 0;
  nSpan_loc = 0;
  if (nDim == 2){
    nSpanWiseSections[marker_flag-1] = 1;
    //TODO (turbo) make it more genral
    if(marker_flag == OUTFLOW)	config->SetnSpanWiseSections(1);

    /*---Initilize the vector of span-wise values that will be ordered ---*/
    SpanWiseValue[marker_flag -1] = new su2double[1];
    for (iSpan = 0; iSpan < 1; iSpan++){
      SpanWiseValue[marker_flag -1][iSpan] = 0;
    }
  }
  else{
    if(SpanWise_Kind == AUTOMATIC){
      /*--- loop to find inflow of outflow marker---*/
      for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
          if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
            if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
              for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
                iPoint = vertex[iMarker][iVertex]->GetNode();

                /*--- loop to find the vertex that ar both of inflow or outflow marker and on the periodic
                 * in order to caount the number of Span ---*/
                for (jMarker = 0; jMarker < nMarker; jMarker++){
                  if (config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) {
                    SendRecv = config->GetMarker_All_SendRecv(jMarker);
                    jVertex = node[iPoint]->GetVertex(jMarker);
                    if (jVertex != -1) {
                      isPeriodic = ((vertex[jMarker][jVertex]->GetRotation_Type() > 0) && (vertex[jMarker][jVertex]->GetRotation_Type() % 2 == 1));
                      if (isPeriodic && (SendRecv < 0)){
                        nSpan++;

                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      /*--- storing the local number of span---*/
      nSpan_loc = nSpan;
      /*--- if parallel computing the global number of span---*/
#ifdef HAVE_MPI
      nSpan_max = nSpan;
      My_nSpan						 = nSpan;											nSpan								 = 0;
      My_MaxnSpan          = nSpan_max;                     nSpan_max            = 0;
      SU2_MPI::Allreduce(&My_nSpan, &nSpan, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
      SU2_MPI::Allreduce(&My_MaxnSpan, &nSpan_max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
#endif



      /*--- initialize the vector that will contain the disordered values span-wise ---*/
      nSpanWiseSections[marker_flag -1] = nSpan;
      valueSpan = new su2double[nSpan];

      for (iSpan = 0; iSpan < nSpan; iSpan ++ ){
        valueSpan[iSpan] = -1001.0;
      }


      /*--- store the local span-wise value for each processor ---*/
      nSpan_loc = 0;
      for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
          if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
            if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
              for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
                iPoint = vertex[iMarker][iVertex]->GetNode();
                for (jMarker = 0; jMarker < nMarker; jMarker++){
                  if (config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) {
                    SendRecv = config->GetMarker_All_SendRecv(jMarker);
                    jVertex = node[iPoint]->GetVertex(jMarker);
                    if (jVertex != -1) {
                      isPeriodic = ((vertex[jMarker][jVertex]->GetRotation_Type() > 0) && (vertex[jMarker][jVertex]->GetRotation_Type() % 2 == 1));
                      if (isPeriodic && (SendRecv < 0)){
                        coord = node[iPoint]->GetCoord();
                        switch (config->GetKind_TurboMachinery(val_iZone)){
                        case CENTRIFUGAL:
                          valueSpan[nSpan_loc] = coord[2];
                          break;
                        case CENTRIPETAL:
                          valueSpan[nSpan_loc] = coord[2];
                          break;
                        case AXIAL:
                          valueSpan[nSpan_loc] = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                          break;
                        case CENTRIPETAL_AXIAL:
                          if (marker_flag == OUTFLOW){
                            valueSpan[nSpan_loc] = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                          }
                          else{
                            valueSpan[nSpan_loc] = coord[2];
                          }
                          break;
                        case AXIAL_CENTRIFUGAL:
                          if (marker_flag == INFLOW){
                            valueSpan[nSpan_loc] = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                          }
                          else{
                            valueSpan[nSpan_loc] = coord[2];
                          }
                          break;

                        }
                        nSpan_loc++;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      /*--- Gather the span-wise values on all the processor ---*/

#ifdef HAVE_MPI
      MyTotValueSpan    				= new su2double[nSpan_max*size];
      MyValueSpan								= new su2double[nSpan_max];
      My_nSpan_loc							= new int[size];
      for(iSpan = 0; iSpan < nSpan_max; iSpan++){
        MyValueSpan[iSpan] = -1001.0;
        for (iSize = 0; iSize< size; iSize++){
          MyTotValueSpan[iSize*nSpan_max + iSpan] = -1001.0;
        }
      }

      for(iSpan = 0; iSpan <nSpan_loc; iSpan++){
        MyValueSpan[iSpan] = valueSpan[iSpan];
      }

      for(iSpan = 0; iSpan <nSpan; iSpan++){
        valueSpan[iSpan] = -1001.0;
      }

      SU2_MPI::Allgather(MyValueSpan, nSpan_max , MPI_DOUBLE, MyTotValueSpan, nSpan_max, MPI_DOUBLE, MPI_COMM_WORLD);
      SU2_MPI::Allgather(&nSpan_loc, 1 , MPI_INT, My_nSpan_loc, 1, MPI_INT, MPI_COMM_WORLD);

      jSpan = 0;
      for (iSize = 0; iSize< size; iSize++){
        for(iSpan = 0; iSpan < My_nSpan_loc[iSize]; iSpan++){
          valueSpan[jSpan] = MyTotValueSpan[iSize*nSpan_max + iSpan];
          jSpan++;
        }
      }

      delete [] MyTotValueSpan; delete [] MyValueSpan; delete [] My_nSpan_loc;

#endif

      // check if the value are gathered correctly
      //
      //  for (iSpan = 0; iSpan < nSpan; iSpan++){
      //  	if(rank == MASTER_NODE){
      //  		cout << setprecision(16)<<  iSpan +1 << " with a value of " <<valueSpan[iSpan]<< " at flag " << marker_flag <<endl;
      //  	}
      //  }


      /*--- Find the minimum value among the span-wise values  ---*/
      min = 10.0E+06;
      for (iSpan = 0; iSpan < nSpan; iSpan++){
        if(valueSpan[iSpan]< min) min = valueSpan[iSpan];
      }

      /*---Initilize the vector of span-wise values that will be ordered ---*/
      SpanWiseValue[marker_flag -1] = new su2double[nSpan];
      for (iSpan = 0; iSpan < nSpan; iSpan++){
        SpanWiseValue[marker_flag -1][iSpan] = 0;
      }

      /*---Ordering the vector of span-wise values---*/
      SpanWiseValue[marker_flag -1][0] = min;
      for (iSpan = 1; iSpan < nSpan; iSpan++){
        min = 10.0E+06;
        for (jSpan = 0; jSpan < nSpan; jSpan++){
          if((valueSpan[jSpan] - SpanWiseValue[marker_flag -1][iSpan-1]) < min && (valueSpan[jSpan] - SpanWiseValue[marker_flag -1][iSpan-1]) > EPS){
            min    = valueSpan[jSpan] - SpanWiseValue[marker_flag -1][iSpan-1];
            kSpan = jSpan;
          }
        }
        SpanWiseValue[marker_flag -1][iSpan] = valueSpan[kSpan];
      }

      delete [] valueSpan;
    }
    /*--- Compute equispaced Span-wise sections using number of section specified by the User---*/
    else{
      /*--- Initialize number of span---*/
      nSpanWiseSections[marker_flag-1] = config->Get_nSpanWiseSections_User();
      SpanWiseValue[marker_flag -1] = new su2double[config->Get_nSpanWiseSections_User()];
      for (iSpan = 0; iSpan < config->Get_nSpanWiseSections_User(); iSpan++){
        SpanWiseValue[marker_flag -1][iSpan] = 0;
      }
      /*--- Compute maximum and minimum value span-wise---*/
      min = 10.0E+06;
      max = -10.0E+06;
      for (iMarker = 0; iMarker < nMarker; iMarker++){
        for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
          if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
            if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
              for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
                iPoint = vertex[iMarker][iVertex]->GetNode();
                for (jMarker = 0; jMarker < nMarker; jMarker++){
                  if (config->GetMarker_All_KindBC(jMarker) == SEND_RECEIVE) {
                    SendRecv = config->GetMarker_All_SendRecv(jMarker);
                    jVertex = node[iPoint]->GetVertex(jMarker);
                    if (jVertex != -1) {
                      isPeriodic = ((vertex[jMarker][jVertex]->GetRotation_Type() > 0) && (vertex[jMarker][jVertex]->GetRotation_Type() % 2 == 1));
                      if (isPeriodic && (SendRecv < 0)){
                        coord = node[iPoint]->GetCoord();
                        switch (config->GetKind_TurboMachinery(val_iZone)){
                        case CENTRIFUGAL: case CENTRIPETAL:
                          if (coord[2] < min) min = coord[2];
                          if (coord[2] > max) max = coord[2];
                          break;
                        case AXIAL:
                          radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                          if (radius < min) min = radius;
                          if (radius > max) max = radius;
                          break;
                        case CENTRIPETAL_AXIAL:
                          if (marker_flag == OUTFLOW){
                            radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                            if (radius < min) min = radius;
                            if (radius > max) max = radius;
                          }
                          else{
                            if (coord[2] < min) min = coord[2];
                            if (coord[2] > max) max = coord[2];
                          }
                          break;

                        case AXIAL_CENTRIFUGAL:
                          if (marker_flag == INFLOW){
                            radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                            if (radius < min) min = radius;
                            if (radius > max) max = radius;
                          }
                          else{
                            if (coord[2] < min) min = coord[2];
                            if (coord[2] > max) max = coord[2];
                          }
                          break;
                        }

                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      /*--- compute global minimum and maximum value on span-wise ---*/
#ifdef HAVE_MPI
      MyMin= min;			min = 0;
      MyMax= max;			max = 0;
      SU2_MPI::Allreduce(&MyMin, &min, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
      SU2_MPI::Allreduce(&MyMax, &max, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
#endif

      //  	cout <<"min  " <<  min << endl;
      //  	cout <<"max  " << max << endl;
      /*--- compute height value for each spanwise section---*/
      delta = (max - min)/(nSpanWiseSections[marker_flag-1] -1);
      for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
        SpanWiseValue[marker_flag - 1][iSpan]= min + delta*iSpan;
      }
    }


    if(marker_flag == OUTFLOW){
      if(nSpanWiseSections[INFLOW -1] != nSpanWiseSections[OUTFLOW - 1]){
        if(rank == MASTER_NODE){
          cout << " At the moment only turbomachinery with the same amount of span-wise section can be simulated" << endl;
          cout <<"nSpan inflow "<< nSpanWiseSections[INFLOW] << " nSpan outflow " << nSpanWiseSections[OUTFLOW]<<endl;
        }
        exit(EXIT_FAILURE);
      }
      else{
        config->SetnSpanWiseSections(nSpanWiseSections[OUTFLOW -1]);
      }
    }



  }

}
void CPhysicalGeometry::SetTurboVertex(CConfig *config, unsigned short val_iZone, unsigned short marker_flag, bool allocate) {
  unsigned long  iPoint, **ordered, **disordered, **oldVertex3D, iInternalVertex;
  unsigned long nVert, nVertMax;
  unsigned short iMarker, iMarkerTP, iSpan, jSpan, iDim;
  su2double min, minInt, max, *coord, dist, Normal2, *TurboNormal, *NormalArea, target = 0.0, **area, ***unitnormal, Area = 0.0;
  int rank = MASTER_NODE;
  bool **checkAssign;
  min    =  10.0E+06;
  minInt =  10.0E+06;
  max    = -10.0E+06;
  
  su2double radius;
  long iVertex, iSpanVertex, jSpanVertex, kSpanVertex = 0;
  int *nTotVertex_gb, *nVertexSpanHalo;
  su2double **x_loc, **y_loc, **z_loc, **angCoord_loc, **deltaAngCoord_loc, **angPitch, **deltaAngPitch, *minIntAngPitch,
  *minAngPitch, *maxAngPitch;
  int       **rank_loc;
#ifdef HAVE_MPI
  unsigned short iSize, kSize = 0, jSize;
  int size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  su2double MyMin,MyIntMin, MyMax;
  su2double *x_gb = NULL, *y_gb = NULL, *z_gb = NULL, *angCoord_gb = NULL, *deltaAngCoord_gb = NULL;
  bool *checkAssign_gb =NULL;
  unsigned long My_nVert;

#endif
  string multizone_filename;

  x_loc              = new su2double*[nSpanWiseSections[marker_flag-1]];
  y_loc              = new su2double*[nSpanWiseSections[marker_flag-1]];
  z_loc              = new su2double*[nSpanWiseSections[marker_flag-1]];
  angCoord_loc       = new su2double*[nSpanWiseSections[marker_flag-1]];
  deltaAngCoord_loc  = new su2double*[nSpanWiseSections[marker_flag-1]];
  angPitch           = new su2double*[nSpanWiseSections[marker_flag-1]];
  deltaAngPitch      = new su2double*[nSpanWiseSections[marker_flag-1]];
  rank_loc           = new int*[nSpanWiseSections[marker_flag-1]];
  minAngPitch        = new su2double[nSpanWiseSections[marker_flag-1]];
  minIntAngPitch     = new su2double[nSpanWiseSections[marker_flag-1]];
  maxAngPitch        = new su2double[nSpanWiseSections[marker_flag-1]];

  nTotVertex_gb      = new int[nSpanWiseSections[marker_flag-1]];
  nVertexSpanHalo    = new int[nSpanWiseSections[marker_flag-1]];
  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
    nTotVertex_gb[iSpan]   = -1;
    nVertexSpanHalo[iSpan] = 0;
    minAngPitch[iSpan]     = 10.0E+06;
    minIntAngPitch[iSpan]  = 10.0E+06;
    maxAngPitch[iSpan]     = -10.0E+06;
  }

  /*--- Initialize auxiliary pointers ---*/
  TurboNormal        = new su2double[3];
  NormalArea         = new su2double[3];
  ordered            = new unsigned long* [nSpanWiseSections[marker_flag-1]];
  disordered         = new unsigned long* [nSpanWiseSections[marker_flag-1]];
  oldVertex3D        = new unsigned long* [nSpanWiseSections[marker_flag-1]];
  area               = new su2double* [nSpanWiseSections[marker_flag-1]];
  unitnormal         = new su2double** [nSpanWiseSections[marker_flag-1]];
  checkAssign        = new bool* [nSpanWiseSections[marker_flag-1]];

  /*--- Initialize the new Vertex structure. The if statement ensures that these vectors are initialized
   * only once even if the routine is called more than once.---*/

  if (allocate){
    for (iMarker = 0; iMarker < nMarker; iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
            nVertexSpan[iMarker]                 = new long[nSpanWiseSections[marker_flag-1]];
            turbovertex[iMarker]                 = new CTurboVertex** [nSpanWiseSections[marker_flag-1]];
            nTotVertexSpan[iMarker]              = new unsigned long [nSpanWiseSections[marker_flag-1] +1];
            MaxAngularCoord[iMarker]             = new su2double [nSpanWiseSections[marker_flag-1]];
            MinAngularCoord[iMarker]             = new su2double [nSpanWiseSections[marker_flag-1]];
            MinRelAngularCoord[iMarker]          = new su2double [nSpanWiseSections[marker_flag-1]];
            for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
              nVertexSpan[iMarker][iSpan]        = 0;
              turbovertex[iMarker][iSpan]        = NULL;
              MinAngularCoord[iMarker][iSpan]    = 10.0E+06;
              MaxAngularCoord[iMarker][iSpan]    = -10.0E+06;
              MinRelAngularCoord[iMarker][iSpan] = 10.0E+06;
            }
            for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1] +1; iSpan++){
              nTotVertexSpan[iMarker][iSpan]     = 0;
            }
          }
        }
      }
    }
  }

  //this works only for turbomachinery rotating around the Z-Axes.
  // the reordering algorithm pitch-wise assumes that X-coordinate of each boundary vertex is positive so that reordering can be based on the Y-coordinate.
    for (iMarker = 0; iMarker < nMarker; iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){

            /*--- compute the amount of vertexes for each span-wise section to initialize the CTurboVertex pointers and auxiliary pointers  ---*/
            for (iVertex = 0; (unsigned long)iVertex  < nVertex[iMarker]; iVertex++) {
              iPoint = vertex[iMarker][iVertex]->GetNode();
              if (nDim == 3){
                dist = 10E+06;
                jSpan = -1;
                coord = node[iPoint]->GetCoord();

                switch (config->GetKind_TurboMachinery(val_iZone)){
                case CENTRIFUGAL: case CENTRIPETAL:
                  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                    if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                      dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                      jSpan=iSpan;
                    }
                  }
                  break;
                case AXIAL:
                  radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                    if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                      dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                      jSpan=iSpan;
                    }
                  }
                  break;
                case CENTRIPETAL_AXIAL:
                  if (marker_flag == OUTFLOW){
                    radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  else{
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  break;

                case AXIAL_CENTRIFUGAL:
                  if (marker_flag == INFLOW){
                    radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  else{
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  break;
                }
              }

              /*--- 2D problem do not need span-wise separation---*/
              else{
                jSpan = 0;
              }

              if(node[iPoint]->GetDomain()){
                nVertexSpan[iMarker][jSpan]++;
              }
              nVertexSpanHalo[jSpan]++;
            }

            /*--- initialize the CTurboVertex pointers and auxiliary pointers  ---*/
            for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
              if (allocate){
                turbovertex[iMarker][iSpan] = new CTurboVertex* [nVertexSpan[iMarker][iSpan]];
                for (iVertex = 0; iVertex < nVertexSpan[iMarker][iSpan]; iVertex++){
                  turbovertex[iMarker][iSpan][iVertex] = NULL;
                }
              }
              ordered[iSpan]                           = new unsigned long [nVertexSpanHalo[iSpan]];
              disordered[iSpan]                        = new unsigned long [nVertexSpanHalo[iSpan]];
              oldVertex3D[iSpan]                       = new unsigned long [nVertexSpanHalo[iSpan]];
              checkAssign[iSpan]                       = new bool [nVertexSpanHalo[iSpan]];
              area[iSpan]                              = new su2double [nVertexSpanHalo[iSpan]];
              unitnormal[iSpan]                        = new su2double* [nVertexSpanHalo[iSpan]];
              for (iVertex = 0; iVertex < nVertexSpanHalo[iSpan]; iVertex++){
                unitnormal[iSpan][iVertex]             = new su2double [nDim];
              }
              angPitch[iSpan]                          = new su2double [nVertexSpanHalo[iSpan]];
              deltaAngPitch[iSpan]                     = new su2double [nVertexSpanHalo[iSpan]];
              nVertexSpanHalo[iSpan]                   = 0;
            }

            /*--- store the vertexes in a ordered manner in span-wise directions but not yet ordered pitch-wise ---*/
            for (iVertex = 0; (unsigned long)iVertex < nVertex[iMarker]; iVertex++) {
              iPoint = vertex[iMarker][iVertex]->GetNode();
              if(nDim == 3){
                dist  = 10E+06;
                jSpan = -1;

                coord = node[iPoint]->GetCoord();
                switch (config->GetKind_TurboMachinery(val_iZone)){
                case CENTRIFUGAL: case CENTRIPETAL:
                  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                    if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                      dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                      jSpan=iSpan;
                    }
                  }
                  break;
                case AXIAL:
                  radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                    if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                      dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                      jSpan=iSpan;
                    }
                  }
                  break;
                case CENTRIPETAL_AXIAL:
                  if(marker_flag == OUTFLOW){
                    radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }else{
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  break;

                case AXIAL_CENTRIFUGAL:
                  if(marker_flag == INFLOW){
                    radius = sqrt(coord[0]*coord[0]+coord[1]*coord[1]);
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(radius - SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(radius-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }else{
                    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
                      if (dist > (abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]))){
                        dist= abs(coord[2]-SpanWiseValue[marker_flag-1][iSpan]);
                        jSpan=iSpan;
                      }
                    }
                  }
                  break;
                }
              }
              /*--- 2D problem do not need span-wise separation---*/
              else{
                jSpan = 0;
              }
              /*--- compute the face area associated with the vertex ---*/
              vertex[iMarker][iVertex]->GetNormal(NormalArea);
              for (iDim = 0; iDim < nDim; iDim++) NormalArea[iDim] = -NormalArea[iDim];
              Area = 0.0;
              for (iDim = 0; iDim < nDim; iDim++)
                Area += NormalArea[iDim]*NormalArea[iDim];
              Area = sqrt(Area);
              for (iDim = 0; iDim < nDim; iDim++) NormalArea[iDim] /= Area;
              /*--- store all the all the info into the auxiliary containers ---*/
              disordered[jSpan][nVertexSpanHalo[jSpan]]  = iPoint;
              oldVertex3D[jSpan][nVertexSpanHalo[jSpan]] = iVertex;
              area[jSpan][nVertexSpanHalo[jSpan]]        = Area;
              for (iDim = 0; iDim < nDim; iDim++){
                unitnormal[jSpan][nVertexSpanHalo[jSpan]][iDim] = NormalArea[iDim];
              }
              checkAssign[jSpan][nVertexSpanHalo[jSpan]] = false;
              nVertexSpanHalo[jSpan]++;
            }

            /*--- using the auxiliary container reordered the vertexes pitch-wise direction at each span ---*/
            // the reordering algorithm can be based on the Y-coordinate.
            for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){

              /*--- find the local minimum and maximum pitch-wise for each processor---*/
              min    = 10E+06;
              minInt = 10E+06;
              max    = -10E+06;
              for(iSpanVertex = 0; iSpanVertex < nVertexSpanHalo[iSpan]; iSpanVertex++){
                iPoint = disordered[iSpan][iSpanVertex];
                coord = node[iPoint]->GetCoord();
                /*--- find nodes at minimum pitch among all nodes---*/
                if (coord[1]<min){
                  min = coord[1];
                  if (nDim == 2 && config->GetKind_TurboMachinery(val_iZone) == AXIAL){
                    MinAngularCoord[iMarker][iSpan] = coord[1];
                  }
                  else{
                  MinAngularCoord[iMarker][iSpan] = atan(coord[1]/coord[0]);
                  }
                  minAngPitch[iSpan]= MinAngularCoord[iMarker][iSpan];
                  kSpanVertex =iSpanVertex;
                }

                /*--- find nodes at minimum pitch among the internal nodes---*/
                if (coord[1]<minInt){
                  if(node[iPoint]->GetDomain()){
                    minInt = coord[1];
                    if (nDim == 2 && config->GetKind_TurboMachinery(val_iZone) == AXIAL){
                      minIntAngPitch[iSpan] = coord[1];
                    }
                    else{
                      minIntAngPitch[iSpan] = atan(coord[1]/coord[0]);
                    }
                  }
                }

                /*--- find nodes at maximum pitch among the internal nodes---*/
                if (coord[1]>max){
                  if(node[iPoint]->GetDomain()){
                    max =coord[1];
                    if (nDim == 2 && config->GetKind_TurboMachinery(val_iZone) == AXIAL){
                      MaxAngularCoord[iMarker][iSpan] = coord[1];
                    }
                    else{
                      MaxAngularCoord[iMarker][iSpan] = atan(coord[1]/coord[0]);
                    }
                    maxAngPitch[iSpan]= MaxAngularCoord[iMarker][iSpan];
                  }
                }
              }

              iInternalVertex = 0;

              /*--- reordering the vertex pitch-wise, store the ordered vertexes span-wise and pitch-wise---*/
              for(iSpanVertex = 0; iSpanVertex<nVertexSpanHalo[iSpan]; iSpanVertex++){
                dist = 10E+06;
                ordered[iSpan][iSpanVertex] = disordered[iSpan][kSpanVertex];
                checkAssign[iSpan][kSpanVertex] = true;
                coord = node[ordered[iSpan][iSpanVertex]]->GetCoord();
                target = coord[1];
                if (nDim == 2 && config->GetKind_TurboMachinery(val_iZone) == AXIAL){
                   angPitch[iSpan][iSpanVertex]=coord[1];
                }
                else{
                  angPitch[iSpan][iSpanVertex]=atan(coord[1]/coord[0]);
                }
                if(iSpanVertex == 0){
                  deltaAngPitch[iSpan][iSpanVertex]=0.0;
                }
                else{
                  deltaAngPitch[iSpan][iSpanVertex]= angPitch[iSpan][iSpanVertex] - angPitch[iSpan][iSpanVertex - 1];
                }
                /*---create turbovertex structure only for the internal nodes---*/
                if(node[ordered[iSpan][iSpanVertex]]->GetDomain()){
                  if (allocate){
                    turbovertex[iMarker][iSpan][iInternalVertex] = new CTurboVertex(ordered[iSpan][iSpanVertex], nDim);
                  }
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetArea(area[iSpan][kSpanVertex]);
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetNormal(unitnormal[iSpan][kSpanVertex]);
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetOldVertex(oldVertex3D[iSpan][kSpanVertex]);
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetAngularCoord(angPitch[iSpan][iSpanVertex]);
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetDeltaAngularCoord(deltaAngPitch[iSpan][iSpanVertex]);
                  switch (config->GetKind_TurboMachinery(val_iZone)){
                  case CENTRIFUGAL:
                    Normal2 = 0.0;
                    for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                    if (marker_flag == INFLOW){
                      TurboNormal[0] = -coord[0]/sqrt(Normal2);
                      TurboNormal[1] = -coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }else{
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }
                    break;
                  case CENTRIPETAL:
                    Normal2 = 0.0;
                    for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                    if (marker_flag == OUTFLOW){
                      TurboNormal[0] = -coord[0]/sqrt(Normal2);
                      TurboNormal[1] = -coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }else{
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }
                    break;
                  case AXIAL:
                    Normal2 = 0.0;
                    for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                    if(nDim == 3){
                      if (marker_flag == INFLOW){
                        TurboNormal[0] = coord[0]/sqrt(Normal2);
                        TurboNormal[1] = coord[1]/sqrt(Normal2);
                        TurboNormal[2] = 0.0;
                      }else{
                        TurboNormal[0] = coord[0]/sqrt(Normal2);
                        TurboNormal[1] = coord[1]/sqrt(Normal2);
                        TurboNormal[2] = 0.0;
                      }
                    }
                    else{
                      if (marker_flag == INFLOW){
                        TurboNormal[0] = -1.0;
                        TurboNormal[1] = 0.0;
                        TurboNormal[2] = 0.0;
                      }else{
                        TurboNormal[0] = 1.0;
                        TurboNormal[1] = 0.0;
                        TurboNormal[2] = 0.0;
                      }
                    }

                    break;
                  case CENTRIPETAL_AXIAL:
                    Normal2 = 0.0;
                    for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                    if (marker_flag == INFLOW){
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }else{
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }
                    break;

                  case AXIAL_CENTRIFUGAL:
                    Normal2 = 0.0;
                    for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                    if (marker_flag == INFLOW){
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }else{
                      TurboNormal[0] = coord[0]/sqrt(Normal2);
                      TurboNormal[1] = coord[1]/sqrt(Normal2);
                      TurboNormal[2] = 0.0;
                    }
                    break;

                  }
                  turbovertex[iMarker][iSpan][iInternalVertex]->SetTurboNormal(TurboNormal);
                  iInternalVertex++;
                }


                for(jSpanVertex = 0; jSpanVertex<nVertexSpanHalo[iSpan]; jSpanVertex++){
                  coord = node[disordered[iSpan][jSpanVertex]]->GetCoord();
                  if(dist >= (coord[1] - target) && !checkAssign[iSpan][jSpanVertex] && (coord[1] - target) >= 0.0){
                    dist= coord[1] - target;
                    kSpanVertex =jSpanVertex;
                  }
                }
              }
            }

            for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){

              delete [] ordered[iSpan];
              delete [] disordered[iSpan];
              delete [] oldVertex3D[iSpan];
              delete [] checkAssign[iSpan];
              delete [] area[iSpan];
              delete [] angPitch[iSpan];
              delete [] deltaAngPitch[iSpan];

              for(iVertex=0; iVertex < nVertexSpanHalo[iSpan]; iVertex++){
                delete [] unitnormal[iSpan][iVertex];
              }
              delete [] unitnormal[iSpan];
            }
          }
        }
      }
    }

  /*--- to be set for all the processor to initialize an appropriate number of frequency for the NR BC ---*/
  nVertMax = 0;

  /*--- compute global max and min pitch per span ---*/
  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
    nVert    = 0;

#ifdef HAVE_MPI
    MyMin     = minAngPitch[iSpan];      minAngPitch[iSpan]    = 10.0E+6;
    MyIntMin  = minIntAngPitch[iSpan];   minIntAngPitch[iSpan] = 10.0E+6;
    MyMax     = maxAngPitch[iSpan];      maxAngPitch[iSpan]    = -10.0E+6;

    SU2_MPI::Allreduce(&MyMin, &minAngPitch[iSpan], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(&MyIntMin, &minIntAngPitch[iSpan], 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(&MyMax, &maxAngPitch[iSpan], 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
#endif


    /*--- compute the relative angular pitch with respect to the minimum value ---*/

    for (iMarker = 0; iMarker < nMarker; iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
            nVert = nVertexSpan[iMarker][iSpan];
            MinAngularCoord[iMarker][iSpan]    = minAngPitch[iSpan];
            MaxAngularCoord[iMarker][iSpan]    = maxAngPitch[iSpan];
            MinRelAngularCoord[iMarker][iSpan] = minIntAngPitch[iSpan] - minAngPitch[iSpan];
            for(iSpanVertex = 0; iSpanVertex< nVertexSpan[iMarker][iSpan]; iSpanVertex++){
             turbovertex[iMarker][iSpan][iSpanVertex]->SetRelAngularCoord(MinAngularCoord[iMarker][iSpan]);
            }
          }
        }
      }
    }


#ifdef HAVE_MPI
    My_nVert = nVert;nVert = 0;
    SU2_MPI::Allreduce(&My_nVert, &nVert, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif

    /*--- to be set for all the processor to initialize an appropriate number of frequency for the NR BC ---*/
    if(nVert > nVertMax){
      SetnVertexSpanMax(marker_flag,nVert);
    }
    /*--- for all the processor should be known the amount of total turbovertex per span  ---*/
    nTotVertex_gb[iSpan]= nVert;

    for (iMarker = 0; iMarker < nMarker; iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
            nTotVertexSpan[iMarker][iSpan]= nVert;
            nTotVertexSpan[iMarker][nSpanWiseSections[marker_flag-1]]+= nVert;
          }
        }
      }
    }
  }


  /*--- Printing Tec file to check the global ordering of the turbovertex pitch-wise ---*/
  /*--- Send all the info to the MASTERNODE ---*/

  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
    x_loc[iSpan]             = new su2double[nTotVertex_gb[iSpan]];
    y_loc[iSpan]             = new su2double[nTotVertex_gb[iSpan]];
    z_loc[iSpan]             = new su2double[nTotVertex_gb[iSpan]];
    angCoord_loc[iSpan]      = new su2double[nTotVertex_gb[iSpan]];
    deltaAngCoord_loc[iSpan] = new su2double[nTotVertex_gb[iSpan]];
    rank_loc[iSpan]          = new int[nTotVertex_gb[iSpan]];
    for(iSpanVertex = 0; iSpanVertex<nTotVertex_gb[iSpan]; iSpanVertex++){
      x_loc[iSpan][iSpanVertex]             = -1.0;
      y_loc[iSpan][iSpanVertex]             = -1.0;
      z_loc[iSpan][iSpanVertex]             = -1.0;
      angCoord_loc[iSpan][iSpanVertex]      = -1.0;
      deltaAngCoord_loc[iSpan][iSpanVertex] = -1.0;
      rank_loc[iSpan][iSpanVertex]          = -1;
    }
  }

  for (iMarker = 0; iMarker < nMarker; iMarker++){
    for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
      if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
        if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
          for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
            for(iSpanVertex = 0; iSpanVertex<nVertexSpan[iMarker][iSpan]; iSpanVertex++){
              iPoint = turbovertex[iMarker][iSpan][iSpanVertex]->GetNode();
              coord  = node[iPoint]->GetCoord();
              x_loc[iSpan][iSpanVertex]   = coord[0];
              y_loc[iSpan][iSpanVertex]   = coord[1];
              if (nDim == 3){
                z_loc[iSpan][iSpanVertex] = coord[2];
              }
              else{
                z_loc[iSpan][iSpanVertex] = 0.0;
              }
              angCoord_loc[iSpan][iSpanVertex]      = turbovertex[iMarker][iSpan][iSpanVertex]->GetRelAngularCoord();
              deltaAngCoord_loc[iSpan][iSpanVertex] = turbovertex[iMarker][iSpan][iSpanVertex]->GetDeltaAngularCoord();
            }
          }
        }
      }
    }
  }

#ifdef HAVE_MPI

  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
    if (rank == MASTER_NODE){
      x_gb                = new su2double[nTotVertex_gb[iSpan]*size];
      y_gb                = new su2double[nTotVertex_gb[iSpan]*size];
      z_gb                = new su2double[nTotVertex_gb[iSpan]*size];
      angCoord_gb         = new su2double[nTotVertex_gb[iSpan]*size];
      deltaAngCoord_gb    = new su2double[nTotVertex_gb[iSpan]*size];
      checkAssign_gb      = new bool[nTotVertex_gb[iSpan]*size];

     for(iSize= 0; iSize < size; iSize++){
       for(iSpanVertex = 0; iSpanVertex < nTotVertex_gb[iSpan]; iSpanVertex++){
         checkAssign_gb[iSize*nTotVertex_gb[iSpan] + iSpanVertex] = false;
       }
     }
    }
    SU2_MPI::Gather(y_loc[iSpan], nTotVertex_gb[iSpan] , MPI_DOUBLE, y_gb, nTotVertex_gb[iSpan], MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    SU2_MPI::Gather(x_loc[iSpan], nTotVertex_gb[iSpan] , MPI_DOUBLE, x_gb, nTotVertex_gb[iSpan], MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    SU2_MPI::Gather(z_loc[iSpan], nTotVertex_gb[iSpan] , MPI_DOUBLE, z_gb, nTotVertex_gb[iSpan], MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    SU2_MPI::Gather(angCoord_loc[iSpan], nTotVertex_gb[iSpan] , MPI_DOUBLE, angCoord_gb, nTotVertex_gb[iSpan], MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);
    SU2_MPI::Gather(deltaAngCoord_loc[iSpan], nTotVertex_gb[iSpan] , MPI_DOUBLE, deltaAngCoord_gb, nTotVertex_gb[iSpan], MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);

    if (rank == MASTER_NODE){
      for(iSpanVertex = 0; iSpanVertex<nTotVertex_gb[iSpan]; iSpanVertex++){
        x_loc[iSpan][iSpanVertex]             = -1.0;
        y_loc[iSpan][iSpanVertex]             = -1.0;
        z_loc[iSpan][iSpanVertex]             = -1.0;
        angCoord_loc[iSpan][iSpanVertex]      = -1.0;
        deltaAngCoord_loc[iSpan][iSpanVertex] = -1.0;
      }



      min = 10.0E+06;
      for(iSize= 0; iSize < size; iSize++){
        if (angCoord_gb[iSize*nTotVertex_gb[iSpan]] < min && angCoord_gb[iSize*nTotVertex_gb[iSpan]] >= 0.0){
          kSize = iSize;
          min = angCoord_gb[iSize*nTotVertex_gb[iSpan]];
        }
      }

      kSpanVertex = 0;
      for(iSpanVertex = 0; iSpanVertex < nTotVertex_gb[iSpan]; iSpanVertex++){
        x_loc[iSpan][iSpanVertex]              = x_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex];
        y_loc[iSpan][iSpanVertex]              = y_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex];
        z_loc[iSpan][iSpanVertex]              = z_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex];
        angCoord_loc[iSpan][iSpanVertex]       = angCoord_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex];
        deltaAngCoord_loc[iSpan][iSpanVertex]  = deltaAngCoord_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex];
        rank_loc[iSpan][iSpanVertex]           = kSize;
        target = angCoord_loc[iSpan][iSpanVertex];
        checkAssign_gb[kSize*nTotVertex_gb[iSpan] + kSpanVertex] = true;
        min = 10.0E+06;
        for(jSize= 0; jSize < size; jSize++){
          for(jSpanVertex = 0; jSpanVertex < nTotVertex_gb[iSpan]; jSpanVertex++){
            if (angCoord_gb[jSize*nTotVertex_gb[iSpan] + jSpanVertex] < min && (angCoord_gb[jSize*nTotVertex_gb[iSpan] + jSpanVertex] - target) >= 0.0 && !checkAssign_gb[jSize*nTotVertex_gb[iSpan] + jSpanVertex]){
              kSize = jSize;
              kSpanVertex = jSpanVertex;
              min = angCoord_gb[jSize*nTotVertex_gb[iSpan] + jSpanVertex];
            }
          }
        }
      }


      delete [] x_gb;	delete [] y_gb; delete [] z_gb;	 delete [] angCoord_gb; delete [] deltaAngCoord_gb; delete[] checkAssign_gb;

    }
  }

#endif

  if (rank == MASTER_NODE){
    if (marker_flag == INFLOW && val_iZone ==0){
      std::string sPath = "TURBOMACHINERY";
      mode_t nMode = 0733; // UNIX style permissions
      int nError = 0;
#if defined(_WIN32)
      nError = _mkdir(sPath.c_str()); // can be used on Windows
#else
      nError = mkdir(sPath.c_str(),nMode); // can be used on non-Windows
#endif
      if (nError != 0) {
        cout << "TURBOMACHINERY folder creation failed." <<endl;
      }
    }
    if (marker_flag == INFLOW){
      multizone_filename = "TURBOMACHINERY/spanwise_division_inflow.dat";
    }
    else{
      multizone_filename = "TURBOMACHINERY/spanwise_division_outflow.dat";
    }
    char buffer[50];

    if (GetnZone() > 1){
      unsigned short lastindex = multizone_filename.find_last_of(".");
      multizone_filename = multizone_filename.substr(0, lastindex);
      SPRINTF (buffer, "_%d.dat", SU2_TYPE::Int(val_iZone));
      multizone_filename.append(string(buffer));
    }

    // File to print the vector x_loc, y_loc, z_loc, globIdx_loc to check vertex ordering
    ofstream myfile;
    myfile.open (multizone_filename.data(), ios::out | ios::trunc);
    myfile.setf(ios::uppercase | ios::scientific);
    myfile.precision(8);

    myfile << "TITLE = \"Global index visualization file\"" << endl;
    myfile << "VARIABLES =" << endl;
    myfile.width(10); myfile << "\"iSpan\"";
    myfile.width(20); myfile << "\"x_coord\"" ;
    myfile.width(20); myfile << "\"y_coord\"" ;
    myfile.width(20); myfile << "\"z_coord\"" ;
    myfile.width(20); myfile << "\"radius\"" ;
    myfile.width(20); myfile << "\"Relative Angular Coord \"" ;
    myfile.width(20); myfile << "\"Delta Angular Coord \"" ;
    myfile.width(20); myfile << "\"processor\"" <<endl;
    for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
      for(iSpanVertex = 0; iSpanVertex < nTotVertex_gb[iSpan]; iSpanVertex++){
        radius = sqrt(x_loc[iSpan][iSpanVertex]*x_loc[iSpan][iSpanVertex] + y_loc[iSpan][iSpanVertex]*y_loc[iSpan][iSpanVertex]);
        myfile.width(10); myfile << iSpan;
        myfile.width(20); myfile << x_loc[iSpan][iSpanVertex];
        myfile.width(20); myfile << y_loc[iSpan][iSpanVertex];
        myfile.width(20); myfile << z_loc[iSpan][iSpanVertex];
        myfile.width(20); myfile << radius;
        if (nDim ==2 && config->GetKind_TurboMachinery(val_iZone)){
          myfile.width(20); myfile << angCoord_loc[iSpan][iSpanVertex];
          myfile.width(20); myfile << deltaAngCoord_loc[iSpan][iSpanVertex];
        }
        else{
          myfile.width(20); myfile << angCoord_loc[iSpan][iSpanVertex]*180.0/PI_NUMBER;
          myfile.width(20); myfile << deltaAngCoord_loc[iSpan][iSpanVertex]*180.0/PI_NUMBER;
        }
        myfile.width(20); myfile << rank_loc[iSpan][iSpanVertex]<<endl;
      }
      myfile << endl;
    }
  }


  for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
    delete [] x_loc[iSpan];
    delete [] y_loc[iSpan];
    delete [] z_loc[iSpan];
    delete [] angCoord_loc[iSpan];
    delete [] deltaAngCoord_loc[iSpan];
    delete [] rank_loc[iSpan];

  }


  delete [] area;
  delete [] ordered;
  delete [] disordered;
  delete [] oldVertex3D;
  delete [] checkAssign;
  delete [] TurboNormal;
  delete [] unitnormal;
  delete [] NormalArea;
  delete [] x_loc;
  delete [] y_loc;
  delete [] z_loc;
  delete [] angCoord_loc;
  delete [] nTotVertex_gb;
  delete [] nVertexSpanHalo;
  delete [] angPitch;
  delete [] deltaAngPitch;
  delete [] deltaAngCoord_loc;
  delete [] rank_loc;
  delete [] minAngPitch;
  delete [] maxAngPitch;

}


void CPhysicalGeometry::UpdateTurboVertex(CConfig *config, unsigned short val_iZone, unsigned short marker_flag) {
  unsigned short iMarker, iMarkerTP, iSpan, iDim;
  long iSpanVertex, iPoint;
  su2double *coord, *TurboNormal, Normal2;

  /*--- Initialize auxiliary pointers ---*/
  TurboNormal      = new su2double[3];

  for (iMarker = 0; iMarker < nMarker; iMarker++){
    for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
      if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
        if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
          for(iSpan = 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){
            for(iSpanVertex = 0; iSpanVertex<nVertexSpan[iMarker][iSpan]; iSpanVertex++){
              iPoint = turbovertex[iMarker][iSpan][iSpanVertex]->GetNode();
              coord  = node[iPoint]->GetCoord();
              /*--- compute appropriate turbo normal ---*/
              switch (config->GetKind_TurboMachinery(val_iZone)){
              case CENTRIFUGAL:
                Normal2 = 0.0;
                for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                if (marker_flag == INFLOW){
                  TurboNormal[0] = -coord[0]/sqrt(Normal2);
                  TurboNormal[1] = -coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }else{
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }
                break;
              case CENTRIPETAL:
                Normal2 = 0.0;
                for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                if (marker_flag == OUTFLOW){
                  TurboNormal[0] = -coord[0]/sqrt(Normal2);
                  TurboNormal[1] = -coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }else{
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }
                break;
              case AXIAL:
                Normal2 = 0.0;
                for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                if(nDim == 3){
                  if (marker_flag == INFLOW){
                    TurboNormal[0] = coord[0]/sqrt(Normal2);
                    TurboNormal[1] = coord[1]/sqrt(Normal2);
                    TurboNormal[2] = 0.0;
                  }else{
                    TurboNormal[0] = coord[0]/sqrt(Normal2);
                    TurboNormal[1] = coord[1]/sqrt(Normal2);
                    TurboNormal[2] = 0.0;
                  }
                }
                else{
                  if (marker_flag == INFLOW){
                    TurboNormal[0] = -1.0;
                    TurboNormal[1] = 0.0;
                    TurboNormal[2] = 0.0;
                  }else{
                    TurboNormal[0] = 1.0;
                    TurboNormal[1] = 0.0;
                    TurboNormal[2] = 0.0;
                  }
                }

                break;
              case CENTRIPETAL_AXIAL:
                Normal2 = 0.0;
                for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                if (marker_flag == INFLOW){
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }else{
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }
                break;

              case AXIAL_CENTRIFUGAL:
                Normal2 = 0.0;
                for(iDim = 0; iDim < 2; iDim++) Normal2 +=coord[iDim]*coord[iDim];
                if (marker_flag == INFLOW){
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }else{
                  TurboNormal[0] = coord[0]/sqrt(Normal2);
                  TurboNormal[1] = coord[1]/sqrt(Normal2);
                  TurboNormal[2] = 0.0;
                }
                break;
              }

              /*--- store the new turbo normal ---*/
              turbovertex[iMarker][iSpan][iSpanVertex]->SetTurboNormal(TurboNormal);
            }
          }
        }
      }
    }
  }

  delete [] TurboNormal;
}

void CPhysicalGeometry::SetAvgTurboValue(CConfig *config, unsigned short val_iZone, unsigned short marker_flag, bool allocate) {

  unsigned short iMarker, iMarkerTP, iSpan, iDim;
  unsigned long iPoint;
  su2double *TurboNormal,*coord, *Normal, turboNormal2, Normal2, *gridVel, TotalArea, TotalRadius, radius;
  su2double *TotalTurboNormal,*TotalNormal, *TotalGridVel, Area;
  long iVertex;
  /*-- Variables declaration and allocation ---*/
  TotalTurboNormal = new su2double[nDim];
  TotalNormal      = new su2double[nDim];
  TurboNormal      = new su2double[nDim];
  TotalGridVel     = new su2double[nDim];
  Normal           = new su2double[nDim];

  bool grid_movement        = config->GetGrid_Movement();
#ifdef HAVE_MPI
  int rank, size;
  su2double MyTotalArea, MyTotalRadius, *MyTotalTurboNormal= NULL, *MyTotalNormal= NULL, *MyTotalGridVel= NULL;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

  /*--- Intialization of the vector for the interested boundary ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++){
    for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
      if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
        if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
          if(allocate){
            AverageTurboNormal[iMarker]               = new su2double *[nSpanWiseSections[marker_flag-1] + 1];
            AverageNormal[iMarker]                    = new su2double *[nSpanWiseSections[marker_flag-1] + 1];
            AverageGridVel[iMarker]                   = new su2double *[nSpanWiseSections[marker_flag-1] + 1];
            AverageTangGridVel[iMarker]               = new su2double [nSpanWiseSections[marker_flag-1] + 1];
            SpanArea[iMarker]                         = new su2double [nSpanWiseSections[marker_flag-1] + 1];
            TurboRadius[iMarker]                      = new su2double [nSpanWiseSections[marker_flag-1] + 1];
            for (iSpan= 0; iSpan < nSpanWiseSections[marker_flag-1] + 1; iSpan++){
              AverageTurboNormal[iMarker][iSpan]      = new su2double [nDim];
              AverageNormal[iMarker][iSpan]           = new su2double [nDim];
              AverageGridVel[iMarker][iSpan]          = new su2double [nDim];
            }
          }
          for (iSpan= 0; iSpan < nSpanWiseSections[marker_flag-1] + 1; iSpan++){
            AverageTangGridVel[iMarker][iSpan]          = 0.0;
            SpanArea[iMarker][iSpan]                    = 0.0;
            TurboRadius[iMarker][iSpan]                 = 0.0;
            for(iDim=0; iDim < nDim; iDim++){
              AverageTurboNormal[iMarker][iSpan][iDim]  = 0.0;
              AverageNormal[iMarker][iSpan][iDim]       = 0.0;
              AverageGridVel[iMarker][iSpan][iDim]      = 0.0;
            }
          }
        }
      }
    }
  }



  /*--- start computing the average quantities span wise --- */
  for (iSpan= 0; iSpan < nSpanWiseSections[marker_flag-1]; iSpan++){

    /*--- Forces initialization for contenitors to zero ---*/
    for (iDim=0; iDim<nDim; iDim++) {
      TotalTurboNormal[iDim]	=0.0;
      TotalNormal[iDim]         =0.0;
      TotalGridVel[iDim]        =0.0;
    }
    TotalArea = 0.0;
    TotalRadius = 0.0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
            for(iVertex = 0; iVertex < nVertexSpan[iMarker][iSpan]; iVertex++){
              iPoint = turbovertex[iMarker][iSpan][iVertex]->GetNode();
              turbovertex[iMarker][iSpan][iVertex]->GetTurboNormal(TurboNormal);
              turbovertex[iMarker][iSpan][iVertex]->GetNormal(Normal);
              coord  = node[iPoint]->GetCoord();

              if (nDim == 3){
                radius = sqrt(coord[0]*coord[0] + coord[1]*coord[1]);
              }
              else{
                radius = 0.0;
              }
              Area = turbovertex[iMarker][iSpan][iVertex]->GetArea();
              TotalArea   += Area;
              TotalRadius += radius;
              for (iDim = 0; iDim < nDim; iDim++) {
                TotalTurboNormal[iDim]  +=TurboNormal[iDim];
                TotalNormal[iDim]       +=Normal[iDim];
              }
              if (grid_movement){
                gridVel = node[iPoint]->GetGridVel();
                for (iDim = 0; iDim < nDim; iDim++) TotalGridVel[iDim] +=gridVel[iDim];
              }
            }
          }
        }
      }
    }

#ifdef HAVE_MPI

    MyTotalArea            = TotalArea;                 TotalArea            = 0;
    MyTotalRadius          = TotalRadius;               TotalRadius          = 0;
    SU2_MPI::Allreduce(&MyTotalArea, &TotalArea, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(&MyTotalRadius, &TotalRadius, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    MyTotalTurboNormal     = new su2double[nDim];
    MyTotalNormal          = new su2double[nDim];
    MyTotalGridVel         = new su2double[nDim];

    for (iDim = 0; iDim < nDim; iDim++) {
      MyTotalTurboNormal[iDim]	= TotalTurboNormal[iDim];
      TotalTurboNormal[iDim]    = 0.0;
      MyTotalNormal[iDim]       = TotalNormal[iDim];
      TotalNormal[iDim]         = 0.0;
      MyTotalGridVel[iDim]      = TotalGridVel[iDim];
      TotalGridVel[iDim]        = 0.0;
    }

    SU2_MPI::Allreduce(MyTotalTurboNormal, TotalTurboNormal, nDim, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(MyTotalNormal, TotalNormal, nDim, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    SU2_MPI::Allreduce(MyTotalGridVel, TotalGridVel, nDim, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    delete [] MyTotalTurboNormal;delete [] MyTotalNormal; delete [] MyTotalGridVel;

#endif

    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++){
      for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){


            SpanArea[iMarker][iSpan]           = TotalArea;
            TurboRadius[iMarker][iSpan]        = TotalRadius/nTotVertexSpan[iMarker][iSpan];

            turboNormal2    = 0.0;
            Normal2         = 0.0;
            for (iDim = 0; iDim < nDim; iDim++){
              turboNormal2 += TotalTurboNormal[iDim]*TotalTurboNormal[iDim];
              Normal2      += TotalNormal[iDim]*TotalNormal[iDim];
            }
            for (iDim = 0; iDim < nDim; iDim++){
              AverageTurboNormal[iMarker][iSpan][iDim] = TotalTurboNormal[iDim]/sqrt(turboNormal2);
              AverageNormal[iMarker][iSpan][iDim]      = TotalNormal[iDim]/sqrt(Normal2);
            }
            if (grid_movement){
              for (iDim = 0; iDim < nDim; iDim++){
                AverageGridVel[iMarker][iSpan][iDim]   =TotalGridVel[iDim]/nTotVertexSpan[iMarker][iSpan];
              }
              switch (config->GetKind_TurboMachinery(val_iZone)){
              case CENTRIFUGAL:case CENTRIPETAL:
                if (marker_flag == INFLOW ){
                  AverageTangGridVel[iMarker][iSpan]= -(AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0]);
                }
                else{
                  AverageTangGridVel[iMarker][iSpan]= AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0];
                }
                break;
              case AXIAL:
                if (marker_flag == INFLOW && nDim == 2){
                  AverageTangGridVel[iMarker][iSpan]= -AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1] + AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0];
                }
                else{
                  AverageTangGridVel[iMarker][iSpan]= AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0];
                }
                  break;
              case CENTRIPETAL_AXIAL:
                if (marker_flag == OUTFLOW){
                  AverageTangGridVel[iMarker][iSpan]= (AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0]);
                }
                else{
                  AverageTangGridVel[iMarker][iSpan]= -(AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0]);
                }
                break;
              case AXIAL_CENTRIFUGAL:
                if (marker_flag == INFLOW)
                {
                  AverageTangGridVel[iMarker][iSpan]= AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0];
                }else
                {
                  AverageTangGridVel[iMarker][iSpan]= AverageTurboNormal[iMarker][iSpan][0]*AverageGridVel[iMarker][iSpan][1]-AverageTurboNormal[iMarker][iSpan][1]*AverageGridVel[iMarker][iSpan][0];
                }
                break;

              default:
                cout << "Tang grid velocity NOT IMPLEMENTED YET for this configuration"<<endl;
                exit(EXIT_FAILURE);
                break;
              }
            }

            /*--- Compute the 1D average values ---*/
            AverageTangGridVel[iMarker][nSpanWiseSections[marker_flag-1]]             += AverageTangGridVel[iMarker][iSpan]/nSpanWiseSections[marker_flag-1];
            SpanArea[iMarker][nSpanWiseSections[marker_flag-1]]                       += SpanArea[iMarker][iSpan];
            for(iDim=0; iDim < nDim; iDim++){
              AverageTurboNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim]     += AverageTurboNormal[iMarker][iSpan][iDim];
              AverageNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim]          += AverageNormal[iMarker][iSpan][iDim];
              AverageGridVel[iMarker][nSpanWiseSections[marker_flag-1]][iDim]         += AverageGridVel[iMarker][iSpan][iDim]/nSpanWiseSections[marker_flag-1];

            }
          }
        }
      }
    }
  }

  /*--- Normalize 1D normals---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++){
    for (iMarkerTP=1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
      if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
        if (config->GetMarker_All_TurbomachineryFlag(iMarker) == marker_flag){
          turboNormal2 = 0.0;
          Normal2 		= 0.0;

          for (iDim = 0; iDim < nDim; iDim++){
            turboNormal2 += AverageTurboNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim]*AverageTurboNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim];
            Normal2      += AverageNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim]*AverageNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim];
          }
          for (iDim = 0; iDim < nDim; iDim++){
            AverageTurboNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim] /=sqrt(turboNormal2);
            AverageNormal[iMarker][nSpanWiseSections[marker_flag-1]][iDim] /=sqrt(Normal2);
          }
        }
      }
    }
  }


  delete [] TotalTurboNormal;
  delete [] TotalNormal;
  delete [] TotalGridVel;
  delete [] TurboNormal;
  delete [] Normal;

}


void CPhysicalGeometry::GatherInOutAverageValues(CConfig *config, bool allocate){

  unsigned short iMarker, iMarkerTP;
  unsigned short iSpan, iDim;
  int rank = MASTER_NODE;
  int markerTP;
  su2double nBlades;
  unsigned short nSpanWiseSections = config->GetnSpanWiseSections();

#ifdef HAVE_MPI
  int size = SINGLE_NODE;
  unsigned short i, n1, n2, n1t, n2t;
#endif

  su2double tangGridVelIn, tangGridVelOut;
  su2double areaIn, areaOut, pitchIn, Pitch;
  su2double radiusIn, radiusOut, *turboNormal;

  turboNormal = new su2double[nDim];
  Pitch = 0.0;

  if(allocate){
    for (iMarkerTP=0; iMarkerTP < config->GetnMarker_TurboPerformance(); iMarkerTP++){
      SpanAreaIn[iMarkerTP]       = new su2double[config->GetnSpanMaxAllZones() +1];
      TangGridVelIn[iMarkerTP]    = new su2double[config->GetnSpanMaxAllZones() +1];
      TurboRadiusIn[iMarkerTP]    = new su2double[config->GetnSpanMaxAllZones() +1];
      SpanAreaOut[iMarkerTP]      = new su2double[config->GetnSpanMaxAllZones() +1];
      TangGridVelOut[iMarkerTP]   = new su2double[config->GetnSpanMaxAllZones() +1];
      TurboRadiusOut[iMarkerTP]   = new su2double[config->GetnSpanMaxAllZones() +1];

      for (iSpan= 0; iSpan < config->GetnSpanMaxAllZones() + 1 ; iSpan++){
        SpanAreaIn[iMarkerTP][iSpan]       = 0.0;
        TangGridVelIn[iMarkerTP][iSpan]    = 0.0;
        TurboRadiusIn[iMarkerTP][iSpan]    = 0.0;
        SpanAreaOut[iMarkerTP][iSpan]      = 0.0;
        TangGridVelOut[iMarkerTP][iSpan]   = 0.0;
        TurboRadiusOut[iMarkerTP][iSpan]   = 0.0;
      }
    }
  }



  for (iSpan= 0; iSpan < nSpanWiseSections + 1 ; iSpan++){
#ifdef HAVE_MPI
    unsigned short i, n1, n2, n1t,n2t;
    int size;
    su2double *TurbGeoIn= NULL,*TurbGeoOut= NULL;
    su2double *TotTurbGeoIn = NULL,*TotTurbGeoOut = NULL;
    int *TotMarkerTP;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    n1          = 6;
    n2          = 3;
    n1t         = n1*size;
    n2t         = n2*size;
    TurbGeoIn  = new su2double[n1];
    TurbGeoOut = new su2double[n2];

    for (i=0;i<n1;i++)
      TurbGeoIn[i]    = -1.0;
    for (i=0;i<n2;i++)
      TurbGeoOut[i]   = -1.0;
#endif
    pitchIn           =  0.0;
    areaIn            = -1.0;
    tangGridVelIn     = -1.0;
    radiusIn          = -1.0;
    for(iDim = 0; iDim < nDim; iDim++){
      turboNormal[iDim] = -1.0;
    }

    areaOut           = -1.0;
    tangGridVelOut    = -1.0;
    radiusOut         = -1.0;

    markerTP          = -1;

    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++){
      for (iMarkerTP = 1; iMarkerTP < config->GetnMarker_Turbomachinery()+1; iMarkerTP++){
        if (config->GetMarker_All_Turbomachinery(iMarker) == iMarkerTP){
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == INFLOW){
            markerTP          = iMarkerTP;
            if (iSpan < nSpanWiseSections){
              pitchIn         = MaxAngularCoord[iMarker][iSpan] - MinAngularCoord[iMarker][iSpan];
            }
            areaIn            = SpanArea[iMarker][iSpan];
            tangGridVelIn     = AverageTangGridVel[iMarker][iSpan];
            radiusIn          = TurboRadius[iMarker][iSpan];
            for(iDim = 0; iDim < nDim; iDim++) turboNormal[iDim] = AverageTurboNormal[iMarker][iSpan][iDim];


#ifdef HAVE_MPI
            TurbGeoIn[0]  = areaIn;
            TurbGeoIn[1]  = tangGridVelIn;
            TurbGeoIn[2]  = radiusIn;
            TurbGeoIn[3]  = turboNormal[0];
            TurbGeoIn[4]  = turboNormal[1];
            TurbGeoIn[5]  = pitchIn;
#endif
          }

          /*--- retrieve outlet information ---*/
          if (config->GetMarker_All_TurbomachineryFlag(iMarker) == OUTFLOW){
            if (iSpan < nSpanWiseSections){
              pitchIn       = MaxAngularCoord[iMarker][iSpan] - MinAngularCoord[iMarker][iSpan];
            }
            areaOut         = SpanArea[iMarker][iSpan];
            tangGridVelOut  = AverageTangGridVel[iMarker][iSpan];
            radiusOut       = TurboRadius[iMarker][iSpan];
#ifdef HAVE_MPI
            TurbGeoOut[0]  = areaOut;
            TurbGeoOut[1]  = tangGridVelOut;
            TurbGeoOut[2]  = radiusOut;
#endif
          }
        }
      }
    }

#ifdef HAVE_MPI
    TotTurbGeoIn       = new su2double[n1t];
    TotTurbGeoOut      = new su2double[n2t];
    for (i=0;i<n1t;i++)
      TotTurbGeoIn[i]  = -1.0;
    for (i=0;i<n2t;i++)
      TotTurbGeoOut[i] = -1.0;
    TotMarkerTP = new int[size];
    for(i=0; i<size; i++){
      TotMarkerTP[i]    = -1;
    }

    SU2_MPI::Allgather(TurbGeoIn, n1, MPI_DOUBLE, TotTurbGeoIn, n1, MPI_DOUBLE, MPI_COMM_WORLD);
    SU2_MPI::Allgather(TurbGeoOut, n2, MPI_DOUBLE,TotTurbGeoOut, n2, MPI_DOUBLE, MPI_COMM_WORLD);
    SU2_MPI::Allgather(&markerTP, 1, MPI_INT,TotMarkerTP, 1, MPI_INT, MPI_COMM_WORLD);

    delete [] TurbGeoIn, delete [] TurbGeoOut;


    for (i=0;i<size;i++){
      if(TotTurbGeoIn[n1*i] > 0.0){
        areaIn              = 0.0;
        areaIn              = TotTurbGeoIn[n1*i];
        tangGridVelIn       = 0.0;
        tangGridVelIn       = TotTurbGeoIn[n1*i+1];
        radiusIn            = 0.0;
        radiusIn            = TotTurbGeoIn[n1*i+2];
        turboNormal[0]      = 0.0;
        turboNormal[0]      = TotTurbGeoIn[n1*i+3];
        turboNormal[1]      = 0.0;
        turboNormal[1]      = TotTurbGeoIn[n1*i+4];
        pitchIn             = 0.0;
        pitchIn             = TotTurbGeoIn[n1*i+5];

        markerTP            = -1;
        markerTP            = TotMarkerTP[i];
      }

      if(TotTurbGeoOut[n2*i] > 0.0){
        areaOut             = 0.0;
        areaOut             = TotTurbGeoOut[n2*i];
        tangGridVelOut      = 0.0;
        tangGridVelOut      = TotTurbGeoOut[n2*i+1];
        radiusOut           = 0.0;
        radiusOut           = TotTurbGeoOut[n2*i+2];
      }
    }

    delete [] TotTurbGeoIn, delete [] TotTurbGeoOut; delete [] TotMarkerTP;


#endif

    Pitch +=pitchIn/nSpanWiseSections;

    if (iSpan == nSpanWiseSections) {
      config->SetFreeStreamTurboNormal(turboNormal);
      if (config->GetKind_TurboMachinery(config->GetiZone()) == AXIAL && nDim == 2){
        nBlades = 1/Pitch;
      }
      else{
        nBlades = 2*PI_NUMBER/Pitch;
      }
      config->SetnBlades(config->GetiZone(), nBlades);
    }

    if (rank == MASTER_NODE){
      /*----Quantities needed for computing the turbomachinery performance -----*/
      SpanAreaIn[markerTP -1][iSpan]       = areaIn;
      TangGridVelIn[markerTP -1][iSpan]    = tangGridVelIn;
      TurboRadiusIn[markerTP -1][iSpan]    = radiusIn;

      SpanAreaOut[markerTP -1][iSpan]      = areaOut;
      TangGridVelOut[markerTP -1][iSpan]   = tangGridVelOut;
      TurboRadiusOut[markerTP -1][iSpan]   = radiusOut;
    }
  }
  delete [] turboNormal;

}


void CPhysicalGeometry::SetCoord_CG(void) {
  unsigned short nNode, iDim, iMarker, iNode;
  unsigned long elem_poin, edge_poin, iElem, iEdge;
  su2double **Coord;
  
  /*--- Compute the center of gravity for elements ---*/
  
  for (iElem = 0; iElem<nElem; iElem++) {
    nNode = elem[iElem]->GetnNodes();
    Coord = new su2double* [nNode];
    
    /*--- Store the coordinates for all the element nodes ---*/
    
    for (iNode = 0; iNode < nNode; iNode++) {
      elem_poin = elem[iElem]->GetNode(iNode);
      Coord[iNode] = new su2double [nDim];
      for (iDim = 0; iDim < nDim; iDim++)
        Coord[iNode][iDim]=node[elem_poin]->GetCoord(iDim);
    }
    
    /*--- Compute the element CG coordinates ---*/
    
    elem[iElem]->SetCoord_CG(Coord);
    
    for (iNode = 0; iNode < nNode; iNode++)
      if (Coord[iNode] != NULL) delete[] Coord[iNode];
    if (Coord != NULL) delete[] Coord;
  }
  
  /*--- Center of gravity for face elements ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++)
    for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++) {
      nNode = bound[iMarker][iElem]->GetnNodes();
      Coord = new su2double* [nNode];
      
      /*--- Store the coordinates for all the element nodes ---*/
      
      for (iNode = 0; iNode < nNode; iNode++) {
        elem_poin = bound[iMarker][iElem]->GetNode(iNode);
        Coord[iNode] = new su2double [nDim];
        for (iDim = 0; iDim < nDim; iDim++)
          Coord[iNode][iDim]=node[elem_poin]->GetCoord(iDim);
      }
      /*--- Compute the element CG coordinates ---*/
      
      bound[iMarker][iElem]->SetCoord_CG(Coord);
      for (iNode = 0; iNode < nNode; iNode++)
        if (Coord[iNode] != NULL) delete[] Coord[iNode];
      if (Coord != NULL) delete[] Coord;
    }
  
  /*--- Center of gravity for edges ---*/
  
  for (iEdge = 0; iEdge < nEdge; iEdge++) {
    nNode = edge[iEdge]->GetnNodes();
    Coord = new su2double* [nNode];
    
    /*--- Store the coordinates for all the element nodes ---*/
    
    for (iNode = 0; iNode < nNode; iNode++) {
      edge_poin=edge[iEdge]->GetNode(iNode);
      Coord[iNode] = new su2double [nDim];
      for (iDim = 0; iDim < nDim; iDim++)
        Coord[iNode][iDim]=node[edge_poin]->GetCoord(iDim);
    }
    
    /*--- Compute the edge CG coordinates ---*/
    
    edge[iEdge]->SetCoord_CG(Coord);
    
    for (iNode = 0; iNode < nNode; iNode++)
      if (Coord[iNode] != NULL) delete[] Coord[iNode];
    if (Coord != NULL) delete[] Coord;
  }
}

void CPhysicalGeometry::SetBoundControlVolume(CConfig *config, unsigned short action) {
  unsigned short Neighbor_Node, iMarker, iNode, iNeighbor_Nodes, iDim;
  unsigned long Neighbor_Point, iVertex, iPoint, iElem;
  long iEdge;
  su2double Area, *NormalFace = NULL;
  
  /*--- Update values of faces of the edge ---*/
  
  if (action != ALLOCATE)
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
        vertex[iMarker][iVertex]->SetZeroValues();
  
  su2double *Coord_Edge_CG = new su2double [nDim];
  su2double *Coord_Elem_CG = new su2double [nDim];
  su2double *Coord_Vertex = new su2double [nDim];
  
  /*--- Loop over all the markers ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++)
    
  /*--- Loop over all the boundary elements ---*/
    
    for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++)
      
    /*--- Loop over all the nodes of the boundary ---*/
      
      for (iNode = 0; iNode < bound[iMarker][iElem]->GetnNodes(); iNode++) {
        iPoint = bound[iMarker][iElem]->GetNode(iNode);
        iVertex = node[iPoint]->GetVertex(iMarker);
        
        /*--- Loop over the neighbor nodes, there is a face for each one ---*/
        
        for (iNeighbor_Nodes = 0; iNeighbor_Nodes < bound[iMarker][iElem]->GetnNeighbor_Nodes(iNode); iNeighbor_Nodes++) {
          Neighbor_Node = bound[iMarker][iElem]->GetNeighbor_Nodes(iNode, iNeighbor_Nodes);
          Neighbor_Point = bound[iMarker][iElem]->GetNode(Neighbor_Node);
          
          /*--- Shared edge by the Neighbor Point and the point ---*/
          
          iEdge = FindEdge(iPoint, Neighbor_Point);
          for (iDim = 0; iDim < nDim; iDim++) {
            Coord_Edge_CG[iDim] = edge[iEdge]->GetCG(iDim);
            Coord_Elem_CG[iDim] = bound[iMarker][iElem]->GetCG(iDim);
            Coord_Vertex[iDim] = node[iPoint]->GetCoord(iDim);
          }
          switch (nDim) {
            case 2:
              
              /*--- Store the 2D face ---*/
              
              if (iNode == 0) vertex[iMarker][iVertex]->SetNodes_Coord(Coord_Elem_CG, Coord_Vertex);
              if (iNode == 1) vertex[iMarker][iVertex]->SetNodes_Coord(Coord_Vertex, Coord_Elem_CG);
              break;
            case 3:
              
              /*--- Store the 3D face ---*/
              
              if (iNeighbor_Nodes == 0) vertex[iMarker][iVertex]->SetNodes_Coord(Coord_Elem_CG, Coord_Edge_CG, Coord_Vertex);
              if (iNeighbor_Nodes == 1) vertex[iMarker][iVertex]->SetNodes_Coord(Coord_Edge_CG, Coord_Elem_CG, Coord_Vertex);
              break;
          }
        }
      }
  
  delete[] Coord_Edge_CG;
  delete[] Coord_Elem_CG;
  delete[] Coord_Vertex;
  
  /*--- Check if there is a normal with null area ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker ++)
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      NormalFace = vertex[iMarker][iVertex]->GetNormal();
      Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim]*NormalFace[iDim];
      Area = sqrt(Area);
      if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = EPS*EPS;
    }
  
}

void CPhysicalGeometry::MatchInterface(CConfig *config) {
  
  su2double epsilon = 1e-1;
  
  unsigned short nMarker_InterfaceBound = config->GetnMarker_InterfaceBound();
  
  if (nMarker_InterfaceBound != 0) {
    
    unsigned short iMarker, iDim, jMarker, pMarker = 0;
    unsigned long iVertex, iPoint, pVertex = 0, pPoint = 0, jVertex, jPoint, iPointGlobal, jPointGlobal, jVertex_, pPointGlobal = 0;
    su2double *Coord_i, Coord_j[3], dist = 0.0, mindist, maxdist_local, maxdist_global;
    int iProcessor, pProcessor = 0;
    unsigned long nLocalVertex_Interface = 0, MaxLocalVertex_Interface = 0;
    int rank, nProcessor;
    
#ifndef HAVE_MPI
    rank = MASTER_NODE;
    nProcessor = SINGLE_NODE;
#else
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
#endif
    
    unsigned long *Buffer_Send_nVertex = new unsigned long [1];
    unsigned long *Buffer_Receive_nVertex = new unsigned long [nProcessor];
    
    if (rank == MASTER_NODE) cout << "Set Interface boundary conditions (if any)." << endl;
    
    /*--- Compute the number of vertex that have interfase boundary condition
     without including the ghost nodes ---*/
    
    nLocalVertex_Interface = 0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
      if (config->GetMarker_All_KindBC(iMarker) == INTERFACE_BOUNDARY)
        for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          if (node[iPoint]->GetDomain()) nLocalVertex_Interface ++;
        }
    
    Buffer_Send_nVertex[0] = nLocalVertex_Interface;
    
    /*--- Send Interface vertex information --*/
    
#ifndef HAVE_MPI
    MaxLocalVertex_Interface = nLocalVertex_Interface;
    Buffer_Receive_nVertex[0] = Buffer_Send_nVertex[0];
#else
    SU2_MPI::Allreduce(&nLocalVertex_Interface, &MaxLocalVertex_Interface, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
    
    su2double *Buffer_Send_Coord = new su2double [MaxLocalVertex_Interface*nDim];
    unsigned long *Buffer_Send_Point = new unsigned long [MaxLocalVertex_Interface];
    unsigned long *Buffer_Send_GlobalIndex  = new unsigned long [MaxLocalVertex_Interface];
    unsigned long *Buffer_Send_Vertex  = new unsigned long [MaxLocalVertex_Interface];
    unsigned long *Buffer_Send_Marker  = new unsigned long [MaxLocalVertex_Interface];
    
    su2double *Buffer_Receive_Coord = new su2double [nProcessor*MaxLocalVertex_Interface*nDim];
    unsigned long *Buffer_Receive_Point = new unsigned long [nProcessor*MaxLocalVertex_Interface];
    unsigned long *Buffer_Receive_GlobalIndex = new unsigned long [nProcessor*MaxLocalVertex_Interface];
    unsigned long *Buffer_Receive_Vertex = new unsigned long [nProcessor*MaxLocalVertex_Interface];
    unsigned long *Buffer_Receive_Marker = new unsigned long [nProcessor*MaxLocalVertex_Interface];
    
    unsigned long nBuffer_Coord = MaxLocalVertex_Interface*nDim;
    unsigned long nBuffer_Point = MaxLocalVertex_Interface;
    unsigned long nBuffer_GlobalIndex = MaxLocalVertex_Interface;
    unsigned long nBuffer_Vertex = MaxLocalVertex_Interface;
    unsigned long nBuffer_Marker = MaxLocalVertex_Interface;
    
    for (iVertex = 0; iVertex < MaxLocalVertex_Interface; iVertex++) {
      Buffer_Send_Point[iVertex] = 0;
      Buffer_Send_GlobalIndex[iVertex] = 0;
      Buffer_Send_Vertex[iVertex] = 0;
      Buffer_Send_Marker[iVertex] = 0;
      for (iDim = 0; iDim < nDim; iDim++)
        Buffer_Send_Coord[iVertex*nDim+iDim] = 0.0;
    }
    
    /*--- Copy coordinates and point to the auxiliar vector --*/
    
    nLocalVertex_Interface = 0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
      if (config->GetMarker_All_KindBC(iMarker) == INTERFACE_BOUNDARY)
        for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          iPointGlobal = node[iPoint]->GetGlobalIndex();
          if (node[iPoint]->GetDomain()) {
            Buffer_Send_Point[nLocalVertex_Interface] = iPoint;
            Buffer_Send_GlobalIndex[nLocalVertex_Interface] = iPointGlobal;
            Buffer_Send_Vertex[nLocalVertex_Interface] = iVertex;
            Buffer_Send_Marker[nLocalVertex_Interface] = iMarker;
            for (iDim = 0; iDim < nDim; iDim++)
              Buffer_Send_Coord[nLocalVertex_Interface*nDim+iDim] = node[iPoint]->GetCoord(iDim);
            nLocalVertex_Interface++;
          }
        }
    
#ifndef HAVE_MPI
    for (unsigned long iBuffer_Coord = 0; iBuffer_Coord < nBuffer_Coord; iBuffer_Coord++)
      Buffer_Receive_Coord[iBuffer_Coord] = Buffer_Send_Coord[iBuffer_Coord];
    for (unsigned long iBuffer_Point = 0; iBuffer_Point < nBuffer_Point; iBuffer_Point++)
      Buffer_Receive_Point[iBuffer_Point] = Buffer_Send_Point[iBuffer_Point];
    for (unsigned long iBuffer_GlobalIndex = 0; iBuffer_GlobalIndex < nBuffer_GlobalIndex; iBuffer_GlobalIndex++)
      Buffer_Receive_GlobalIndex[iBuffer_GlobalIndex] = Buffer_Send_GlobalIndex[iBuffer_GlobalIndex];
    for (unsigned long iBuffer_Vertex = 0; iBuffer_Vertex < nBuffer_Vertex; iBuffer_Vertex++)
      Buffer_Receive_Vertex[iBuffer_Vertex] = Buffer_Send_Vertex[iBuffer_Vertex];
    for (unsigned long iBuffer_Marker = 0; iBuffer_Marker < nBuffer_Marker; iBuffer_Marker++)
      Buffer_Receive_Marker[iBuffer_Marker] = Buffer_Send_Marker[iBuffer_Marker];
#else
    SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer_Coord, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer_Coord, MPI_DOUBLE, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Point, nBuffer_Point, MPI_UNSIGNED_LONG, Buffer_Receive_Point, nBuffer_Point, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, Buffer_Receive_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, Buffer_Receive_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, Buffer_Receive_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
    
    
    /*--- Compute the closest point to a Near-Field boundary point ---*/
    
    maxdist_local = 0.0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
      if (config->GetMarker_All_KindBC(iMarker) == INTERFACE_BOUNDARY) {
        
        for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          iPointGlobal = node[iPoint]->GetGlobalIndex();
          
          if (node[iPoint]->GetDomain()) {
            
            /*--- Coordinates of the boundary point ---*/
            
            Coord_i = node[iPoint]->GetCoord(); mindist = 1E6; pProcessor = 0; pPoint = 0;
            
            /*--- Loop over all the boundaries to find the pair ---*/
            for (iProcessor = 0; iProcessor < nProcessor; iProcessor++)
              for (jVertex = 0; jVertex < Buffer_Receive_nVertex[iProcessor]; jVertex++) {
                jPoint = Buffer_Receive_Point[iProcessor*MaxLocalVertex_Interface+jVertex];
                jPointGlobal = Buffer_Receive_GlobalIndex[iProcessor*MaxLocalVertex_Interface+jVertex];
                jVertex_ = Buffer_Receive_Vertex[iProcessor*MaxLocalVertex_Interface+jVertex];
                jMarker = Buffer_Receive_Marker[iProcessor*MaxLocalVertex_Interface+jVertex];
                
                if (jPointGlobal != iPointGlobal) {
                  
                  /*--- Compute the distance ---*/
                  
                  dist = 0.0; for (iDim = 0; iDim < nDim; iDim++) {
                    Coord_j[iDim] = Buffer_Receive_Coord[(iProcessor*MaxLocalVertex_Interface+jVertex)*nDim+iDim];
                    dist += pow(Coord_j[iDim]-Coord_i[iDim],2.0);
                  } dist = sqrt(dist);
                  
                  if (((dist < mindist) && (iProcessor != rank)) ||
                      ((dist < mindist) && (iProcessor == rank) && (jPoint != iPoint))) {
                    mindist = dist; pProcessor = iProcessor; pPoint = jPoint; pPointGlobal = jPointGlobal;
                    pVertex = jVertex_; pMarker = jMarker;
                    if (dist == 0.0) break;
                  }
                }
              }
            
            /*--- Store the value of the pair ---*/
            
            maxdist_local = max(maxdist_local, mindist);
            vertex[iMarker][iVertex]->SetDonorPoint(pPoint, pPointGlobal, pVertex, pMarker, pProcessor);
            
            if (mindist > epsilon) {
              cout.precision(10);
              cout << endl;
              cout << "   Bad match for point " << iPoint << ".\tNearest";
              cout << " donor distance: " << scientific << mindist << ".";
              vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iPointGlobal, pVertex, pMarker, pProcessor);
              maxdist_local = min(maxdist_local, 0.0);
            }
            
          }
        }
      }
    }
    
#ifndef HAVE_MPI
    maxdist_global = maxdist_local;
#else
    SU2_MPI::Reduce(&maxdist_local, &maxdist_global, 1, MPI_DOUBLE, MPI_MAX, MASTER_NODE, MPI_COMM_WORLD);
#endif
    
    if (rank == MASTER_NODE) cout <<"The max distance between points is: " << maxdist_global <<"."<< endl;
    
    delete[] Buffer_Send_Coord;
    delete[] Buffer_Send_Point;
    
    delete[] Buffer_Receive_Coord;
    delete[] Buffer_Receive_Point;
    
    delete[] Buffer_Send_nVertex;
    delete[] Buffer_Receive_nVertex;
    
  }
  
}

void CPhysicalGeometry::MatchNearField(CConfig *config) {
  
  su2double epsilon = 1e-1;
  
  unsigned short nMarker_NearFieldBound = config->GetnMarker_NearFieldBound();
  
  if (nMarker_NearFieldBound != 0) {
    
    unsigned short iMarker, iDim, jMarker, pMarker = 0;
    unsigned long iVertex, iPoint, pVertex = 0, pPoint = 0, jVertex, jPoint, iPointGlobal, jPointGlobal, jVertex_, pPointGlobal = 0;
    su2double *Coord_i, Coord_j[3], dist = 0.0, mindist, maxdist_local, maxdist_global;
    int iProcessor, pProcessor = 0;
    unsigned long nLocalVertex_NearField = 0, MaxLocalVertex_NearField = 0;
    int rank, nProcessor;
    
#ifndef HAVE_MPI
    rank = MASTER_NODE;
    nProcessor = SINGLE_NODE;
#else
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
#endif
    
    unsigned long *Buffer_Send_nVertex = new unsigned long [1];
    unsigned long *Buffer_Receive_nVertex = new unsigned long [nProcessor];
    
    if (rank == MASTER_NODE) cout << "Set NearField boundary conditions (if any)." << endl;
    
    /*--- Compute the number of vertex that have interfase boundary condition
     without including the ghost nodes ---*/
    
    nLocalVertex_NearField = 0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
      if (config->GetMarker_All_KindBC(iMarker) == NEARFIELD_BOUNDARY)
        for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          if (node[iPoint]->GetDomain()) nLocalVertex_NearField ++;
        }
    
    Buffer_Send_nVertex[0] = nLocalVertex_NearField;
    
    /*--- Send NearField vertex information --*/
    
#ifndef HAVE_MPI
    MaxLocalVertex_NearField = nLocalVertex_NearField;
    Buffer_Receive_nVertex[0] = Buffer_Send_nVertex[0];
#else
    SU2_MPI::Allreduce(&nLocalVertex_NearField, &MaxLocalVertex_NearField, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
    
    su2double *Buffer_Send_Coord = new su2double [MaxLocalVertex_NearField*nDim];
    unsigned long *Buffer_Send_Point = new unsigned long [MaxLocalVertex_NearField];
    unsigned long *Buffer_Send_GlobalIndex  = new unsigned long [MaxLocalVertex_NearField];
    unsigned long *Buffer_Send_Vertex  = new unsigned long [MaxLocalVertex_NearField];
    unsigned long *Buffer_Send_Marker  = new unsigned long [MaxLocalVertex_NearField];
    
    su2double *Buffer_Receive_Coord = new su2double [nProcessor*MaxLocalVertex_NearField*nDim];
    unsigned long *Buffer_Receive_Point = new unsigned long [nProcessor*MaxLocalVertex_NearField];
    unsigned long *Buffer_Receive_GlobalIndex = new unsigned long [nProcessor*MaxLocalVertex_NearField];
    unsigned long *Buffer_Receive_Vertex = new unsigned long [nProcessor*MaxLocalVertex_NearField];
    unsigned long *Buffer_Receive_Marker = new unsigned long [nProcessor*MaxLocalVertex_NearField];
    
    unsigned long nBuffer_Coord = MaxLocalVertex_NearField*nDim;
    unsigned long nBuffer_Point = MaxLocalVertex_NearField;
    unsigned long nBuffer_GlobalIndex = MaxLocalVertex_NearField;
    unsigned long nBuffer_Vertex = MaxLocalVertex_NearField;
    unsigned long nBuffer_Marker = MaxLocalVertex_NearField;
    
    for (iVertex = 0; iVertex < MaxLocalVertex_NearField; iVertex++) {
      Buffer_Send_Point[iVertex] = 0;
      Buffer_Send_GlobalIndex[iVertex] = 0;
      Buffer_Send_Vertex[iVertex] = 0;
      Buffer_Send_Marker[iVertex] = 0;
      for (iDim = 0; iDim < nDim; iDim++)
        Buffer_Send_Coord[iVertex*nDim+iDim] = 0.0;
    }
    
    /*--- Copy coordinates and point to the auxiliar vector --*/
    
    nLocalVertex_NearField = 0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
      if (config->GetMarker_All_KindBC(iMarker) == NEARFIELD_BOUNDARY)
        for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          iPointGlobal = node[iPoint]->GetGlobalIndex();
          if (node[iPoint]->GetDomain()) {
            Buffer_Send_Point[nLocalVertex_NearField] = iPoint;
            Buffer_Send_GlobalIndex[nLocalVertex_NearField] = iPointGlobal;
            Buffer_Send_Vertex[nLocalVertex_NearField] = iVertex;
            Buffer_Send_Marker[nLocalVertex_NearField] = iMarker;
            for (iDim = 0; iDim < nDim; iDim++)
              Buffer_Send_Coord[nLocalVertex_NearField*nDim+iDim] = node[iPoint]->GetCoord(iDim);
            nLocalVertex_NearField++;
          }
        }
    
#ifndef HAVE_MPI
    for (unsigned long iBuffer_Coord = 0; iBuffer_Coord < nBuffer_Coord; iBuffer_Coord++)
      Buffer_Receive_Coord[iBuffer_Coord] = Buffer_Send_Coord[iBuffer_Coord];
    for (unsigned long iBuffer_Point = 0; iBuffer_Point < nBuffer_Point; iBuffer_Point++)
      Buffer_Receive_Point[iBuffer_Point] = Buffer_Send_Point[iBuffer_Point];
    for (unsigned long iBuffer_GlobalIndex = 0; iBuffer_GlobalIndex < nBuffer_GlobalIndex; iBuffer_GlobalIndex++)
      Buffer_Receive_GlobalIndex[iBuffer_GlobalIndex] = Buffer_Send_GlobalIndex[iBuffer_GlobalIndex];
    for (unsigned long iBuffer_Vertex = 0; iBuffer_Vertex < nBuffer_Vertex; iBuffer_Vertex++)
      Buffer_Receive_Vertex[iBuffer_Vertex] = Buffer_Send_Vertex[iBuffer_Vertex];
    for (unsigned long iBuffer_Marker = 0; iBuffer_Marker < nBuffer_Marker; iBuffer_Marker++)
      Buffer_Receive_Marker[iBuffer_Marker] = Buffer_Send_Marker[iBuffer_Marker];
#else
    SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer_Coord, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer_Coord, MPI_DOUBLE, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Point, nBuffer_Point, MPI_UNSIGNED_LONG, Buffer_Receive_Point, nBuffer_Point, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, Buffer_Receive_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, Buffer_Receive_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    SU2_MPI::Allgather(Buffer_Send_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, Buffer_Receive_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
    
    
    /*--- Compute the closest point to a Near-Field boundary point ---*/
    
    maxdist_local = 0.0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
      if (config->GetMarker_All_KindBC(iMarker) == NEARFIELD_BOUNDARY) {
        
        for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          iPointGlobal = node[iPoint]->GetGlobalIndex();
          
          if (node[iPoint]->GetDomain()) {
            
            /*--- Coordinates of the boundary point ---*/
            
            Coord_i = node[iPoint]->GetCoord(); mindist = 1E6; pProcessor = 0; pPoint = 0;
            
            /*--- Loop over all the boundaries to find the pair ---*/
            for (iProcessor = 0; iProcessor < nProcessor; iProcessor++)
              for (jVertex = 0; jVertex < Buffer_Receive_nVertex[iProcessor]; jVertex++) {
                jPoint = Buffer_Receive_Point[iProcessor*MaxLocalVertex_NearField+jVertex];
                jPointGlobal = Buffer_Receive_GlobalIndex[iProcessor*MaxLocalVertex_NearField+jVertex];
                jVertex_ = Buffer_Receive_Vertex[iProcessor*MaxLocalVertex_NearField+jVertex];
                jMarker = Buffer_Receive_Marker[iProcessor*MaxLocalVertex_NearField+jVertex];
                
                if (jPointGlobal != iPointGlobal) {
                  
                  /*--- Compute the distance ---*/
                  
                  dist = 0.0; for (iDim = 0; iDim < nDim; iDim++) {
                    Coord_j[iDim] = Buffer_Receive_Coord[(iProcessor*MaxLocalVertex_NearField+jVertex)*nDim+iDim];
                    dist += pow(Coord_j[iDim]-Coord_i[iDim],2.0);
                  } dist = sqrt(dist);
                  
                  if (((dist < mindist) && (iProcessor != rank)) ||
                      ((dist < mindist) && (iProcessor == rank) && (jPoint != iPoint))) {
                    mindist = dist; pProcessor = iProcessor; pPoint = jPoint; pPointGlobal = jPointGlobal;
                    pVertex = jVertex_; pMarker = jMarker;
                    if (dist == 0.0) break;
                  }
                }
              }
            
            /*--- Store the value of the pair ---*/
            
            maxdist_local = max(maxdist_local, mindist);
            vertex[iMarker][iVertex]->SetDonorPoint(pPoint, pPointGlobal, pVertex, pMarker, pProcessor);
            
            if (mindist > epsilon) {
              cout.precision(10);
              cout << endl;
              cout << "   Bad match for point " << iPoint << ".\tNearest";
              cout << " donor distance: " << scientific << mindist << ".";
              vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iPointGlobal, pVertex, pMarker, pProcessor);
              maxdist_local = min(maxdist_local, 0.0);
            }
            
          }
        }
      }
    }
    
#ifndef HAVE_MPI
    maxdist_global = maxdist_local;
#else
    SU2_MPI::Reduce(&maxdist_local, &maxdist_global, 1, MPI_DOUBLE, MPI_MAX, MASTER_NODE, MPI_COMM_WORLD);
#endif
    
    if (rank == MASTER_NODE) cout <<"The max distance between points is: " << maxdist_global <<"."<< endl;
    
    delete[] Buffer_Send_Coord;
    delete[] Buffer_Send_Point;
    
    delete[] Buffer_Receive_Coord;
    delete[] Buffer_Receive_Point;
    
    delete[] Buffer_Send_nVertex;
    delete[] Buffer_Receive_nVertex;
    
  }
  
}

void CPhysicalGeometry::MatchActuator_Disk(CConfig *config) {
  
  su2double epsilon = 1e-1;
  
  unsigned short nMarker_ActDiskInlet = config->GetnMarker_ActDiskInlet();
  
  if (nMarker_ActDiskInlet != 0) {
    
    unsigned short iMarker, iDim;
    unsigned long iVertex, iPoint, iPointGlobal, pPoint = 0, pPointGlobal = 0, pVertex = 0, pMarker = 0, jVertex, jVertex_, jPoint, jPointGlobal, jMarker;
    su2double *Coord_i, Coord_j[3], dist = 0.0, mindist, maxdist_local = 0.0, maxdist_global = 0.0;
    int iProcessor, pProcessor = 0;
    unsigned long nLocalVertex_ActDisk = 0, MaxLocalVertex_ActDisk = 0;
    int rank, nProcessor;
    unsigned short Beneficiary = 0, Donor = 0, iBC;
    bool Perimeter;
    
    for (iBC = 0; iBC < 2; iBC++) {
      
      if (iBC == 0) { Beneficiary = ACTDISK_INLET; Donor = ACTDISK_OUTLET; }
      if (iBC == 1) { Beneficiary = ACTDISK_OUTLET; Donor = ACTDISK_INLET; }
      
#ifndef HAVE_MPI
      rank = MASTER_NODE;
      nProcessor = SINGLE_NODE;
#else
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
#endif
      
      unsigned long *Buffer_Send_nVertex = new unsigned long [1];
      unsigned long *Buffer_Receive_nVertex = new unsigned long [nProcessor];
      
      if ((iBC == 0) && (rank == MASTER_NODE)) cout << "Set Actuator Disk inlet boundary conditions." << endl;
      if ((iBC == 1) && (rank == MASTER_NODE)) cout << "Set Actuator Disk outlet boundary conditions." << endl;
      
      /*--- Compute the number of vertex that have an actuator disk outlet boundary condition
       without including the ghost nodes ---*/
      
      nLocalVertex_ActDisk = 0;
      for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
        if (config->GetMarker_All_KindBC(iMarker) == Donor) {
          for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
            iPoint = vertex[iMarker][iVertex]->GetNode();
            if (node[iPoint]->GetDomain()) nLocalVertex_ActDisk ++;
          }
        }
      }
      
      Buffer_Send_nVertex[0] = nLocalVertex_ActDisk;
      
      /*--- Send actuator disk vertex information --*/
      
#ifndef HAVE_MPI
      MaxLocalVertex_ActDisk = nLocalVertex_ActDisk;
      Buffer_Receive_nVertex[0] = Buffer_Send_nVertex[0];
#else
      SU2_MPI::Allreduce(&nLocalVertex_ActDisk, &MaxLocalVertex_ActDisk, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
      SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
      
      /*--- Array dimensionalization --*/
      
      su2double *Buffer_Send_Coord = new su2double [MaxLocalVertex_ActDisk*nDim];
      unsigned long *Buffer_Send_Point  = new unsigned long [MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Send_GlobalIndex  = new unsigned long [MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Send_Vertex  = new unsigned long [MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Send_Marker  = new unsigned long [MaxLocalVertex_ActDisk];
      
      su2double *Buffer_Receive_Coord = new su2double [nProcessor*MaxLocalVertex_ActDisk*nDim];
      unsigned long *Buffer_Receive_Point = new unsigned long [nProcessor*MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Receive_GlobalIndex = new unsigned long [nProcessor*MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Receive_Vertex = new unsigned long [nProcessor*MaxLocalVertex_ActDisk];
      unsigned long *Buffer_Receive_Marker = new unsigned long [nProcessor*MaxLocalVertex_ActDisk];
      
      unsigned long nBuffer_Coord = MaxLocalVertex_ActDisk*nDim;
      unsigned long nBuffer_Point = MaxLocalVertex_ActDisk;
      unsigned long nBuffer_GlobalIndex = MaxLocalVertex_ActDisk;
      unsigned long nBuffer_Vertex = MaxLocalVertex_ActDisk;
      unsigned long nBuffer_Marker = MaxLocalVertex_ActDisk;
      
      for (iVertex = 0; iVertex < MaxLocalVertex_ActDisk; iVertex++) {
        Buffer_Send_Point[iVertex] = 0;
        Buffer_Send_GlobalIndex[iVertex] = 0;
        Buffer_Send_Vertex[iVertex] = 0;
        Buffer_Send_Marker[iVertex] = 0;
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Send_Coord[iVertex*nDim+iDim] = 0.0;
      }
      
      /*--- Copy coordinates and point to the auxiliar vector --*/
      
      nLocalVertex_ActDisk = 0;
      for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
        if (config->GetMarker_All_KindBC(iMarker) == Donor) {
          for (iVertex = 0; iVertex < GetnVertex(iMarker); iVertex++) {
            iPoint = vertex[iMarker][iVertex]->GetNode();
            iPointGlobal = node[iPoint]->GetGlobalIndex();
            if (node[iPoint]->GetDomain()) {
              Buffer_Send_Point[nLocalVertex_ActDisk] = iPoint;
              Buffer_Send_GlobalIndex[nLocalVertex_ActDisk] = iPointGlobal;
              Buffer_Send_Vertex[nLocalVertex_ActDisk] = iVertex;
              Buffer_Send_Marker[nLocalVertex_ActDisk] = iMarker;
              for (iDim = 0; iDim < nDim; iDim++)
                Buffer_Send_Coord[nLocalVertex_ActDisk*nDim+iDim] = node[iPoint]->GetCoord(iDim);
              nLocalVertex_ActDisk++;
            }
          }
        }
      }
      
#ifndef HAVE_MPI
      for (unsigned long iBuffer_Coord = 0; iBuffer_Coord < nBuffer_Coord; iBuffer_Coord++)
        Buffer_Receive_Coord[iBuffer_Coord] = Buffer_Send_Coord[iBuffer_Coord];
      for (unsigned long iBuffer_Point = 0; iBuffer_Point < nBuffer_Point; iBuffer_Point++)
        Buffer_Receive_Point[iBuffer_Point] = Buffer_Send_Point[iBuffer_Point];
      for (unsigned long iBuffer_GlobalIndex = 0; iBuffer_GlobalIndex < nBuffer_GlobalIndex; iBuffer_GlobalIndex++)
        Buffer_Receive_GlobalIndex[iBuffer_GlobalIndex] = Buffer_Send_GlobalIndex[iBuffer_GlobalIndex];
      for (unsigned long iBuffer_Vertex = 0; iBuffer_Vertex < nBuffer_Vertex; iBuffer_Vertex++)
        Buffer_Receive_Vertex[iBuffer_Vertex] = Buffer_Send_Vertex[iBuffer_Vertex];
      for (unsigned long iBuffer_Marker = 0; iBuffer_Marker < nBuffer_Marker; iBuffer_Marker++)
        Buffer_Receive_Marker[iBuffer_Marker] = Buffer_Send_Marker[iBuffer_Marker];
      
#else
      SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer_Coord, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer_Coord, MPI_DOUBLE, MPI_COMM_WORLD);
      SU2_MPI::Allgather(Buffer_Send_Point, nBuffer_Point, MPI_UNSIGNED_LONG, Buffer_Receive_Point, nBuffer_Point, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      SU2_MPI::Allgather(Buffer_Send_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, Buffer_Receive_GlobalIndex, nBuffer_GlobalIndex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      SU2_MPI::Allgather(Buffer_Send_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, Buffer_Receive_Vertex, nBuffer_Vertex, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      SU2_MPI::Allgather(Buffer_Send_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, Buffer_Receive_Marker, nBuffer_Marker, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
#endif
      
      /*--- Compute the closest point to an actuator disk inlet point ---*/
      
      maxdist_local = 0.0;
      
      for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
        if (config->GetMarker_All_KindBC(iMarker) == Beneficiary) {
          
          for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
            iPoint = vertex[iMarker][iVertex]->GetNode();
            iPointGlobal = node[iPoint]->GetGlobalIndex();
            
            
            if (node[iPoint]->GetDomain()) {
              
              /*--- Coordinates of the boundary point ---*/
              
              Coord_i = node[iPoint]->GetCoord(); mindist = 1E6; pProcessor = 0; pPoint = 0;
              
              /*--- Loop over all the boundaries to find the pair ---*/
              
              Perimeter = false;
              
              for (iProcessor = 0; iProcessor < nProcessor; iProcessor++) {
                for (jVertex = 0; jVertex < Buffer_Receive_nVertex[iProcessor]; jVertex++) {
                  jPoint = Buffer_Receive_Point[iProcessor*MaxLocalVertex_ActDisk+jVertex];
                  jPointGlobal = Buffer_Receive_GlobalIndex[iProcessor*MaxLocalVertex_ActDisk+jVertex];
                  jVertex_ = Buffer_Receive_Vertex[iProcessor*MaxLocalVertex_ActDisk+jVertex];
                  jMarker = Buffer_Receive_Marker[iProcessor*MaxLocalVertex_ActDisk+jVertex];
                  
 //                 if (jPointGlobal != iPointGlobal) {
 //                 ActDisk_Perimeter

                    /*--- Compute the distance ---*/
                    
                    dist = 0.0;
                    for (iDim = 0; iDim < nDim; iDim++) {
                      Coord_j[iDim] = Buffer_Receive_Coord[(iProcessor*MaxLocalVertex_ActDisk+jVertex)*nDim+iDim];
                      dist += pow(Coord_j[iDim]-Coord_i[iDim], 2.0);
                    }
                    dist = sqrt(dist);
                    
                    if (dist < mindist) {
                      mindist = dist; pProcessor = iProcessor; pPoint = jPoint; pPointGlobal = jPointGlobal;
                      pVertex = jVertex_; pMarker = jMarker;
                      if (dist == 0.0) break;
                    }
                    
//                  }
//                  else { Perimeter = true; mindist = 0.0; dist = 0.0; break; }
               }
              }
              
              /*--- Store the value of the pair ---*/
              
              maxdist_local = max(maxdist_local, mindist);
              vertex[iMarker][iVertex]->SetDonorPoint(pPoint, pPointGlobal, pVertex, pMarker, pProcessor);
              vertex[iMarker][iVertex]->SetActDisk_Perimeter(Perimeter);
              
              if (mindist > epsilon) {
                cout.precision(10);
                cout << endl;
                cout << "   Bad match for point " << iPoint << ".\tNearest";
                cout << " donor distance: " << scientific << mindist << ".";
                vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iPointGlobal, pVertex, pMarker, pProcessor);
                maxdist_local = min(maxdist_local, 0.0);
              }
              
            }
          }
          
        }
      }
      
#ifndef HAVE_MPI
      maxdist_global = maxdist_local;
#else
      SU2_MPI::Reduce(&maxdist_local, &maxdist_global, 1, MPI_DOUBLE, MPI_MAX, MASTER_NODE, MPI_COMM_WORLD);
#endif
      
      if (rank == MASTER_NODE) cout <<"The max distance between points is: " << maxdist_global <<"."<< endl;
      
      delete[] Buffer_Send_Coord;
      delete[] Buffer_Send_Point;
      
      delete[] Buffer_Receive_Coord;
      delete[] Buffer_Receive_Point;
      
      delete[] Buffer_Send_nVertex;
      delete[] Buffer_Receive_nVertex;
      
    }
  }
  
}

void CPhysicalGeometry::MatchZone(CConfig *config, CGeometry *geometry_donor, CConfig *config_donor,
                                  unsigned short val_iZone, unsigned short val_nZone) {
  
#ifndef HAVE_MPI
  
  unsigned short iMarker, jMarker;
  unsigned long iVertex, iPoint, jVertex, jPoint = 0, pPoint = 0, pGlobalPoint = 0;
  su2double *Coord_i, *Coord_j, dist = 0.0, mindist, maxdist;
  
//  if (val_iZone == ZONE_0) cout << "Set zone boundary conditions (if any)." << endl;
  
  maxdist = 0.0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      iPoint = vertex[iMarker][iVertex]->GetNode();
      Coord_i = node[iPoint]->GetCoord();
      
      mindist = 1E6;
      for (jMarker = 0; jMarker < config_donor->GetnMarker_All(); jMarker++)
        for (jVertex = 0; jVertex < geometry_donor->GetnVertex(jMarker); jVertex++) {
          jPoint = geometry_donor->vertex[jMarker][jVertex]->GetNode();
          Coord_j = geometry_donor->node[jPoint]->GetCoord();
          if (nDim == 2) dist = sqrt(pow(Coord_j[0]-Coord_i[0],2.0) + pow(Coord_j[1]-Coord_i[1],2.0));
          if (nDim == 3) dist = sqrt(pow(Coord_j[0]-Coord_i[0],2.0) + pow(Coord_j[1]-Coord_i[1],2.0) + pow(Coord_j[2]-Coord_i[2],2.0));
//          if (dist < mindist) { mindist = dist; pPoint = jPoint; pGlobalPoint = node[jPoint]->GetGlobalIndex();}
          if (dist < mindist) { mindist = dist; pPoint = jPoint; pGlobalPoint = geometry_donor->node[jPoint]->GetGlobalIndex();}
        }
      
      maxdist = max(maxdist, mindist);
      vertex[iMarker][iVertex]->SetDonorPoint(pPoint, MASTER_NODE, pGlobalPoint);
      
    }
  }
  
#else
  
  unsigned short iMarker, iDim;
  unsigned long iVertex, iPoint, pPoint = 0, jVertex, jPoint, jGlobalPoint = 0, pGlobalPoint = 0;
  su2double *Coord_i, Coord_j[3], dist = 0.0, mindist, maxdist;
  int iProcessor, pProcessor = 0;
  unsigned long nLocalVertex_Zone = 0, nGlobalVertex_Zone = 0, MaxLocalVertex_Zone = 0;
  int rank, nProcessor;
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
  
  unsigned long *Buffer_Send_nVertex = new unsigned long [1];
  unsigned long *Buffer_Receive_nVertex = new unsigned long [nProcessor];
  
//  if (val_iZone == ZONE_0 && rank == MASTER_NODE) cout << "Set zone boundary conditions (if any)." << endl;
  
  nLocalVertex_Zone = 0;
  for (iMarker = 0; iMarker < config_donor->GetnMarker_All(); iMarker++)
    for (iVertex = 0; iVertex < geometry_donor->GetnVertex(iMarker); iVertex++) {
      iPoint = geometry_donor->vertex[iMarker][iVertex]->GetNode();
      if (geometry_donor->node[iPoint]->GetDomain()) nLocalVertex_Zone ++;
    }
  
  Buffer_Send_nVertex[0] = nLocalVertex_Zone;
  
  /*--- Send Interface vertex information --*/
  
  SU2_MPI::Allreduce(&nLocalVertex_Zone, &nGlobalVertex_Zone, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&nLocalVertex_Zone, &MaxLocalVertex_Zone, 1, MPI_UNSIGNED_LONG, MPI_MAX, MPI_COMM_WORLD);
  SU2_MPI::Allgather(Buffer_Send_nVertex, 1, MPI_UNSIGNED_LONG, Buffer_Receive_nVertex, 1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
  
  su2double *Buffer_Send_Coord = new su2double [MaxLocalVertex_Zone*nDim];
  unsigned long *Buffer_Send_Point = new unsigned long [MaxLocalVertex_Zone];
  unsigned long *Buffer_Send_GlobalPoint = new unsigned long [MaxLocalVertex_Zone];
  
  su2double *Buffer_Receive_Coord = new su2double [nProcessor*MaxLocalVertex_Zone*nDim];
  unsigned long *Buffer_Receive_Point = new unsigned long [nProcessor*MaxLocalVertex_Zone];
  unsigned long *Buffer_Receive_GlobalPoint = new unsigned long [nProcessor*MaxLocalVertex_Zone];
  
  unsigned long nBuffer_Coord = MaxLocalVertex_Zone*nDim;
  unsigned long nBuffer_Point = MaxLocalVertex_Zone;
  

  for (iVertex = 0; iVertex < MaxLocalVertex_Zone; iVertex++) {
    Buffer_Send_Point[iVertex] = 0;
    Buffer_Send_GlobalPoint[iVertex] = 0;
    for (iDim = 0; iDim < nDim; iDim++)
      Buffer_Send_Coord[iVertex*nDim+iDim] = 0.0;
  }
  
  /*--- Copy coordinates and point to the auxiliar vector --*/
  nLocalVertex_Zone = 0;
  for (iMarker = 0; iMarker < config_donor->GetnMarker_All(); iMarker++)
    for (iVertex = 0; iVertex < geometry_donor->GetnVertex(iMarker); iVertex++) {
      iPoint = geometry_donor->vertex[iMarker][iVertex]->GetNode();
      if (geometry_donor->node[iPoint]->GetDomain()) {
        Buffer_Send_Point[nLocalVertex_Zone] = iPoint;
        Buffer_Send_GlobalPoint[nLocalVertex_Zone] = geometry_donor->node[iPoint]->GetGlobalIndex();
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Send_Coord[nLocalVertex_Zone*nDim+iDim] = geometry_donor->node[iPoint]->GetCoord(iDim);
        nLocalVertex_Zone++;
      }
    }
  
  SU2_MPI::Allgather(Buffer_Send_Coord, nBuffer_Coord, MPI_DOUBLE, Buffer_Receive_Coord, nBuffer_Coord, MPI_DOUBLE, MPI_COMM_WORLD);
  SU2_MPI::Allgather(Buffer_Send_Point, nBuffer_Point, MPI_UNSIGNED_LONG, Buffer_Receive_Point, nBuffer_Point, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
  SU2_MPI::Allgather(Buffer_Send_GlobalPoint, nBuffer_Point, MPI_UNSIGNED_LONG, Buffer_Receive_GlobalPoint, nBuffer_Point, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);

  /*--- Compute the closest point to a Near-Field boundary point ---*/
  maxdist = 0.0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      iPoint = vertex[iMarker][iVertex]->GetNode();
      
      if (node[iPoint]->GetDomain()) {
        
        /*--- Coordinates of the boundary point ---*/
        Coord_i = node[iPoint]->GetCoord(); mindist = 1E6; pProcessor = 0; pPoint = 0;
        
        /*--- Loop over all the boundaries to find the pair ---*/
        for (iProcessor = 0; iProcessor < nProcessor; iProcessor++)
          for (jVertex = 0; jVertex < Buffer_Receive_nVertex[iProcessor]; jVertex++) {
            jPoint = Buffer_Receive_Point[iProcessor*MaxLocalVertex_Zone+jVertex];
            jGlobalPoint = Buffer_Receive_GlobalPoint[iProcessor*MaxLocalVertex_Zone+jVertex];

            /*--- Compute the distance ---*/
            dist = 0.0; for (iDim = 0; iDim < nDim; iDim++) {
              Coord_j[iDim] = Buffer_Receive_Coord[(iProcessor*MaxLocalVertex_Zone+jVertex)*nDim+iDim];
              dist += pow(Coord_j[iDim]-Coord_i[iDim],2.0);
            } dist = sqrt(dist);
            
            if (((dist < mindist) && (iProcessor != rank)) ||
                ((dist < mindist) && (iProcessor == rank) && (jPoint != iPoint))) {
              mindist = dist; pProcessor = iProcessor; pPoint = jPoint;
              pGlobalPoint = jGlobalPoint;
            }
          }
        
        /*--- Store the value of the pair ---*/
        maxdist = max(maxdist, mindist);
        vertex[iMarker][iVertex]->SetDonorPoint(pPoint, pProcessor, pGlobalPoint);
        
        
      }
    }
  }
  
  delete[] Buffer_Send_Coord;
  delete[] Buffer_Send_Point;
  delete[] Buffer_Send_GlobalPoint;
  
  delete[] Buffer_Receive_Coord;
  delete[] Buffer_Receive_Point;
  delete[] Buffer_Receive_GlobalPoint;
  
  delete[] Buffer_Send_nVertex;
  delete[] Buffer_Receive_nVertex;
  
#endif
  
}


void CPhysicalGeometry::SetControlVolume(CConfig *config, unsigned short action) {
  unsigned long face_iPoint = 0, face_jPoint = 0, iPoint, iElem;
  long iEdge;
  unsigned short nEdgesFace = 1, iFace, iEdgesFace, iDim;
  su2double *Coord_Edge_CG, *Coord_FaceElem_CG, *Coord_Elem_CG, *Coord_FaceiPoint, *Coord_FacejPoint, Area,
  Volume, DomainVolume, my_DomainVolume, *NormalFace = NULL;
  bool change_face_orientation;
  int rank;
  
#ifndef HAVE_MPI
  rank = MASTER_NODE;
#else
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  /*--- Update values of faces of the edge ---*/
  if (action != ALLOCATE) {
    for (iEdge = 0; iEdge < (long)nEdge; iEdge++)
      edge[iEdge]->SetZeroValues();
    for (iPoint = 0; iPoint < nPoint; iPoint++)
      node[iPoint]->SetVolume (0.0);
  }
  
  Coord_Edge_CG = new su2double [nDim];
  Coord_FaceElem_CG = new su2double [nDim];
  Coord_Elem_CG = new su2double [nDim];
  Coord_FaceiPoint = new su2double [nDim];
  Coord_FacejPoint = new su2double [nDim];
  
  my_DomainVolume = 0.0;
  for (iElem = 0; iElem < nElem; iElem++)
    for (iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++) {
      
      /*--- In 2D all the faces have only one edge ---*/
      if (nDim == 2) nEdgesFace = 1;
      /*--- In 3D the number of edges per face is the same as the number of point per face ---*/
      if (nDim == 3) nEdgesFace = elem[iElem]->GetnNodesFace(iFace);
      
      /*-- Loop over the edges of a face ---*/
      for (iEdgesFace = 0; iEdgesFace < nEdgesFace; iEdgesFace++) {
        
        /*--- In 2D only one edge (two points) per edge ---*/
        if (nDim == 2) {
          face_iPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,0));
          face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,1));
        }
        
        /*--- In 3D there are several edges in each face ---*/
        if (nDim == 3) {
          face_iPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace, iEdgesFace));
          if (iEdgesFace != nEdgesFace-1)
            face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace, iEdgesFace+1));
          else
            face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,0));
        }
        
        /*--- We define a direction (from the smalest index to the greatest) --*/
        change_face_orientation = false;
        if (face_iPoint > face_jPoint) change_face_orientation = true;
        iEdge = FindEdge(face_iPoint, face_jPoint);
        
        for (iDim = 0; iDim < nDim; iDim++) {
          Coord_Edge_CG[iDim] = edge[iEdge]->GetCG(iDim);
          Coord_Elem_CG[iDim] = elem[iElem]->GetCG(iDim);
          Coord_FaceElem_CG[iDim] = elem[iElem]->GetFaceCG(iFace, iDim);
          Coord_FaceiPoint[iDim] = node[face_iPoint]->GetCoord(iDim);
          Coord_FacejPoint[iDim] = node[face_jPoint]->GetCoord(iDim);
        }
        
        switch (nDim) {
          case 2:
            /*--- Two dimensional problem ---*/
            if (change_face_orientation) edge[iEdge]->SetNodes_Coord(Coord_Elem_CG, Coord_Edge_CG);
            else edge[iEdge]->SetNodes_Coord(Coord_Edge_CG, Coord_Elem_CG);
            Area = edge[iEdge]->GetVolume(Coord_FaceiPoint, Coord_Edge_CG, Coord_Elem_CG);
            node[face_iPoint]->AddVolume(Area); my_DomainVolume +=Area;
            Area = edge[iEdge]->GetVolume(Coord_FacejPoint, Coord_Edge_CG, Coord_Elem_CG);
            node[face_jPoint]->AddVolume(Area); my_DomainVolume +=Area;
            break;
          case 3:
            /*--- Three dimensional problem ---*/
            if (change_face_orientation) edge[iEdge]->SetNodes_Coord(Coord_FaceElem_CG, Coord_Edge_CG, Coord_Elem_CG);
            else edge[iEdge]->SetNodes_Coord(Coord_Edge_CG, Coord_FaceElem_CG, Coord_Elem_CG);
            Volume = edge[iEdge]->GetVolume(Coord_FaceiPoint, Coord_Edge_CG, Coord_FaceElem_CG, Coord_Elem_CG);
            node[face_iPoint]->AddVolume(Volume); my_DomainVolume +=Volume;
            Volume = edge[iEdge]->GetVolume(Coord_FacejPoint, Coord_Edge_CG, Coord_FaceElem_CG, Coord_Elem_CG);
            node[face_jPoint]->AddVolume(Volume); my_DomainVolume +=Volume;
            break;
        }
      }
    }
  
  /*--- Check if there is a normal with null area ---*/
  for (iEdge = 0; iEdge < (long)nEdge; iEdge++) {
    NormalFace = edge[iEdge]->GetNormal();
    Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim]*NormalFace[iDim];
    Area = sqrt(Area);
    if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = EPS*EPS;
  }
  
  
#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&my_DomainVolume, &DomainVolume, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#else
  DomainVolume = my_DomainVolume;
#endif
  
  if ((rank == MASTER_NODE) && (action == ALLOCATE)) {
    if (nDim == 2) cout <<"Area of the computational grid: "<< DomainVolume <<"."<< endl;
    if (nDim == 3) cout <<"Volume of the computational grid: "<< DomainVolume <<"."<< endl;
  }
  
  config->SetDomainVolume(DomainVolume);
  
  delete[] Coord_Edge_CG;
  delete[] Coord_FaceElem_CG;
  delete[] Coord_Elem_CG;
  delete[] Coord_FaceiPoint;
  delete[] Coord_FacejPoint;
}

void CPhysicalGeometry::VisualizeControlVolume(CConfig *config, unsigned short action) {
  
  /*--- This routine is only meant for visualization in serial currently ---*/
#ifndef HAVE_MPI
  
  unsigned long face_iPoint = 0, face_jPoint = 0, iElem, iPoint_Viz;
  long iEdge;
  unsigned short nEdgesFace = 1, iFace, iEdgesFace, iDim;
  su2double *Coord_Edge_CG, *Coord_FaceElem_CG, *Coord_Elem_CG, *Coord_FaceiPoint,
  *Coord_FacejPoint;
  int counter = 0;
  char cstr[MAX_STRING_SIZE], buffer[50];
  ofstream Tecplot_File;
  string mesh_filename;
  vector<su2double> X, Y, Z, X_n, Y_n, Z_n;
  su2double r1[3], r2[3], CrossProduct[3];
  
  /*--- Access the point number for control volume we want to vizualize ---*/
  
  iPoint_Viz = config->GetVisualize_CV();
  
  /*--- Allocate some structures for building the dual CVs ---*/
  
  Coord_Edge_CG     = new su2double [nDim];
  Coord_FaceElem_CG = new su2double [nDim];
  Coord_Elem_CG     = new su2double [nDim];
  Coord_FaceiPoint  = new su2double [nDim];
  Coord_FacejPoint  = new su2double [nDim];
  
  /*--- Loop over each face of each element ---*/
  
  CrossProduct[0] = 0.0; CrossProduct[1] = 0.0; CrossProduct[2] = 0.0;
  
  for (iElem = 0; iElem < nElem; iElem++) {
    
    for (iFace = 0; iFace < elem[iElem]->GetnFaces(); iFace++) {
      
      /*--- In 2D all the faces have only one edge ---*/
      if (nDim == 2) nEdgesFace = 1;
      /*--- In 3D the number of edges per face is the same as the number of point per face ---*/
      if (nDim == 3) nEdgesFace = elem[iElem]->GetnNodesFace(iFace);
      
      /*-- Loop over the edges of a face ---*/
      for (iEdgesFace = 0; iEdgesFace < nEdgesFace; iEdgesFace++) {
        
        /*--- In 2D only one edge (two points) per edge ---*/
        if (nDim == 2) {
          face_iPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,0));
          face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,1));
        }
        
        /*--- In 3D there are several edges in each face ---*/
        if (nDim == 3) {
          face_iPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace, iEdgesFace));
          if (iEdgesFace != nEdgesFace-1)
            face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace, iEdgesFace+1));
          else
            face_jPoint = elem[iElem]->GetNode(elem[iElem]->GetFaces(iFace,0));
        }
        
        /*--- We define a direction (from the smallest index to the greatest) --*/
        iEdge = FindEdge(face_iPoint, face_jPoint);
        
        for (iDim = 0; iDim < nDim; iDim++) {
          Coord_Edge_CG[iDim] = edge[iEdge]->GetCG(iDim);
          Coord_Elem_CG[iDim] = elem[iElem]->GetCG(iDim);
          Coord_FaceElem_CG[iDim] = elem[iElem]->GetFaceCG(iFace, iDim);
          Coord_FaceiPoint[iDim] = node[face_iPoint]->GetCoord(iDim);
          Coord_FacejPoint[iDim] = node[face_jPoint]->GetCoord(iDim);
        }
        
        /*--- Print out the coordinates for a set of triangles making
         up a single dual control volume for visualization. ---*/

        if (face_iPoint == iPoint_Viz || face_jPoint == iPoint_Viz) {

          if (nDim == 2) {
            X.push_back(Coord_Elem_CG[0]); X.push_back(Coord_Edge_CG[0]);
            Y.push_back(Coord_Elem_CG[1]); Y.push_back(Coord_Edge_CG[1]);
          } else if (nDim == 3) {
            X.push_back(Coord_FaceElem_CG[0]); X.push_back(Coord_Edge_CG[0]); X.push_back(Coord_Elem_CG[0]);
            Y.push_back(Coord_FaceElem_CG[1]); Y.push_back(Coord_Edge_CG[1]); Y.push_back(Coord_Elem_CG[1]);
            Z.push_back(Coord_FaceElem_CG[2]); Z.push_back(Coord_Edge_CG[2]); Z.push_back(Coord_Elem_CG[2]);

            for (iDim = 0; iDim < nDim; iDim++) {
              r1[iDim] = Coord_FaceElem_CG[iDim]-Coord_Elem_CG[iDim];
              r2[iDim] = Coord_Edge_CG[iDim]-Coord_Elem_CG[iDim];
            }
            CrossProduct[0] += 0.5*(r1[1]*r2[2] - r1[2]*r2[1]);
            CrossProduct[1] += 0.5*(r1[2]*r2[0] - r1[0]*r2[2]);
            CrossProduct[2] += 0.5*(r1[0]*r2[1] - r1[1]*r2[0]);
          }
          counter++;
        }
      }
    }
  }
  
  /*--- Write a Tecplot file to visualize the CV ---*/
  
  strcpy(cstr,"dual_cv");
  SPRINTF (buffer, "_%d.dat", SU2_TYPE::Int(iPoint_Viz));
  strcat(cstr, buffer);
  
  Tecplot_File.open(cstr, ios::out);
  Tecplot_File << "TITLE= \"Visualization of the control volume\"" << endl;
  
  if (nDim == 2) {
    Tecplot_File << "VARIABLES = \"x\",\"y\" " << endl;
    Tecplot_File << "ZONE NODES= "<< counter*2 <<", ELEMENTS= ";
    Tecplot_File << counter <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
  } if (nDim == 3) {
    Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\" " << endl;
    Tecplot_File << "ZONE NODES= "<< counter*3 <<", ELEMENTS= ";
    Tecplot_File << counter <<", DATAPACKING=POINT, ZONETYPE=FEBRICK"<< endl;
  }
  
  /*--- Write coordinates for the nodes in the order that they were found
   for each of the edges/triangles making up a dual control volume. ---*/
  
  for (vector<su2double>::size_type i = 0; i != X.size(); i++) {
    Tecplot_File << X[i] << "\t" << Y[i];
    if (nDim == 3) Tecplot_File << "\t" << Z[i];
    Tecplot_File << "\n";
  }
  
  /*--- Create a new connectivity table in the order the faces were found ---*/
  
  int j;
  for (int i= 0; i < counter; i++) {
    if (nDim == 2) {
      j = i*2;
      Tecplot_File << j+1 <<"\t"<<j+2 <<"\t"<<j+2 <<"\t"<<j+2 << endl;
    } if (nDim == 3) {
      j = i*3;
      Tecplot_File << j+1 <<"\t"<<j+2 <<"\t"<<j+3 <<"\t"<<j+3 <<"\t";
      Tecplot_File << j+3<<"\t" <<j+3 <<"\t"<<j+3 <<"\t"<<j+3 << endl;
    }
  }
  
  Tecplot_File.close();
  X.clear();
  Y.clear();
  Z.clear();
  
  delete[] Coord_Edge_CG;
  delete[] Coord_FaceElem_CG;
  delete[] Coord_Elem_CG;
  delete[] Coord_FaceiPoint;
  delete[] Coord_FacejPoint;
  
#endif

}

void CPhysicalGeometry::SetMeshFile (CConfig *config, string val_mesh_out_filename) {
  unsigned long iElem, iPoint, iElem_Bound;
  unsigned short iMarker, iNodes, iDim;
  unsigned short iPeriodic, nPeriodic = 0;
  ofstream output_file;
  string Grid_Marker;
  char *cstr;
  su2double *center, *angles, *transl;
  
  cstr = new char [val_mesh_out_filename.size()+1];
  strcpy (cstr, val_mesh_out_filename.c_str());
  
  /*--- Open .su2 grid file ---*/
  
  output_file.precision(15);
  output_file.open(cstr, ios::out);
  
  /*--- Write dimension, number of elements and number of points ---*/
  
  output_file << "NDIME= " << nDim << endl;
  output_file << "NELEM= " << nElem << endl;
  for (iElem = 0; iElem < nElem; iElem++) {
    output_file << elem[iElem]->GetVTK_Type();
    for (iNodes = 0; iNodes < elem[iElem]->GetnNodes(); iNodes++)
      output_file << "\t" << elem[iElem]->GetNode(iNodes);
    output_file << "\t"<<iElem<< endl;
  }
  
  /*--- Write the node coordinates ---*/
  
  output_file << "NPOIN= " << nPoint << "\t" << nPointDomain << endl;
  output_file.precision(15);
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++)
      output_file << scientific << "\t" << node[iPoint]->GetCoord(iDim) ;
#ifndef HAVE_MPI
    output_file << "\t" << iPoint << endl;
#else
    output_file << "\t" << iPoint << "\t" << node[iPoint]->GetGlobalIndex() << endl;
#endif
    
  }
  
  /*--- Loop through and write the boundary info ---*/
  
  output_file << "NMARK= " << nMarker << endl;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    
    /*--- Ignore SEND_RECEIVE for the moment ---*/
    if (bound[iMarker][0]->GetVTK_Type() != VERTEX) {
      
      Grid_Marker = config->GetMarker_All_TagBound(iMarker);
      output_file << "MARKER_TAG= " << Grid_Marker << endl;
      output_file << "MARKER_ELEMS= " << nElem_Bound[iMarker]<< endl;
      
      if (nDim == 2) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          output_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
          for (iNodes = 0; iNodes < bound[iMarker][iElem_Bound]->GetnNodes(); iNodes++)
            output_file << bound[iMarker][iElem_Bound]->GetNode(iNodes) << "\t" ;
          output_file	<< iElem_Bound << endl;
        }
      }
      
      if (nDim == 3) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          output_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
          for (iNodes = 0; iNodes < bound[iMarker][iElem_Bound]->GetnNodes(); iNodes++)
            output_file << bound[iMarker][iElem_Bound]->GetNode(iNodes) << "\t" ;
          output_file	<< iElem_Bound << endl;
        }
      }
      
    } else if (bound[iMarker][0]->GetVTK_Type() == VERTEX) {
      output_file << "MARKER_TAG= SEND_RECEIVE" << endl;
      output_file << "MARKER_ELEMS= " << nElem_Bound[iMarker]<< endl;
      if (config->GetMarker_All_SendRecv(iMarker) > 0) output_file << "SEND_TO= " << config->GetMarker_All_SendRecv(iMarker) << endl;
      if (config->GetMarker_All_SendRecv(iMarker) < 0) output_file << "SEND_TO= " << config->GetMarker_All_SendRecv(iMarker) << endl;
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        output_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" <<
        bound[iMarker][iElem_Bound]->GetNode(0) << "\t" <<
        bound[iMarker][iElem_Bound]->GetRotation_Type() << endl;
      }
      
    }
  }
  
  /*--- Get the total number of periodic transformations ---*/
  
  nPeriodic = config->GetnPeriodicIndex();
  output_file << "NPERIODIC= " << nPeriodic << endl;
  
  /*--- From iPeriodic obtain the iMarker ---*/
  
  for (iPeriodic = 0; iPeriodic < nPeriodic; iPeriodic++) {
    
    /*--- Retrieve the supplied periodic information. ---*/
    
    center = config->GetPeriodicCenter(iPeriodic);
    angles = config->GetPeriodicRotation(iPeriodic);
    transl = config->GetPeriodicTranslate(iPeriodic);
    
    output_file << "PERIODIC_INDEX= " << iPeriodic << endl;
    output_file << center[0] << "\t" << center[1] << "\t" << center[2] << endl;
    output_file << angles[0] << "\t" << angles[1] << "\t" << angles[2] << endl;
    output_file << transl[0] << "\t" << transl[1] << "\t" << transl[2] << endl;
    
  }
  
  
  output_file.close();
}

void CPhysicalGeometry::SetCoord_Smoothing (unsigned short val_nSmooth, su2double val_smooth_coeff, CConfig *config) {
  unsigned short iSmooth, nneigh, iMarker;
  su2double *Coord_Old, *Coord_Sum, *Coord, *Coord_i, *Coord_j, Position_Plane = 0.0;
  unsigned long iEdge, iPoint, jPoint, iVertex;
  su2double eps = 1E-6;
  bool NearField = false;
  
  Coord = new su2double [nDim];
  
  for (iPoint = 0; iPoint < GetnPoint(); iPoint++) {
    su2double *Coord = node[iPoint]->GetCoord();
    node[iPoint]->SetCoord_Old(Coord);
  }
  
  /*--- Jacobi iterations ---*/
  for (iSmooth = 0; iSmooth < val_nSmooth; iSmooth++) {
    
    for (iPoint = 0; iPoint < nPoint; iPoint++)
      node[iPoint]->SetCoord_SumZero();
    
    
    /*--- Loop over Interior edges ---*/
    for (iEdge = 0; iEdge < nEdge; iEdge++) {
      iPoint = edge[iEdge]->GetNode(0);
      Coord_i = node[iPoint]->GetCoord();
      
      jPoint = edge[iEdge]->GetNode(1);
      Coord_j = node[jPoint]->GetCoord();
      
      /*--- Accumulate nearest neighbor Coord to Res_sum for each variable ---*/
      node[iPoint]->AddCoord_Sum(Coord_j);
      node[jPoint]->AddCoord_Sum(Coord_i);
      
    }
    
    /*--- Loop over all mesh points (Update Coords with averaged sum) ---*/
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      nneigh = node[iPoint]->GetnPoint();
      Coord_Sum = node[iPoint]->GetCoord_Sum();
      Coord_Old = node[iPoint]->GetCoord_Old();
      
      if (nDim == 2) {
        Coord[0] =(Coord_Old[0] + val_smooth_coeff*Coord_Sum[0]) /(1.0 + val_smooth_coeff*su2double(nneigh));
        Coord[1] =(Coord_Old[1] + val_smooth_coeff*Coord_Sum[1]) /(1.0 + val_smooth_coeff*su2double(nneigh));
        if ((NearField) && ((Coord_Old[1] > Position_Plane-eps) && (Coord_Old[1] < Position_Plane+eps)))
          Coord[1] = Coord_Old[1];
      }
      
      if (nDim == 3) {
        Coord[0] =(Coord_Old[0] + val_smooth_coeff*Coord_Sum[0]) /(1.0 + val_smooth_coeff*su2double(nneigh));
        Coord[1] =(Coord_Old[1] + val_smooth_coeff*Coord_Sum[1]) /(1.0 + val_smooth_coeff*su2double(nneigh));
        Coord[2] =(Coord_Old[2] + val_smooth_coeff*Coord_Sum[2]) /(1.0 + val_smooth_coeff*su2double(nneigh));
        if ((NearField) && ((Coord_Old[2] > Position_Plane-eps) && (Coord_Old[2] < Position_Plane+eps)))
          Coord[2] = Coord_Old[2];
      }
      
      node[iPoint]->SetCoord(Coord);
    }
    
    /*--- Copy boundary values ---*/
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        Coord_Old = node[iPoint]->GetCoord_Old();
        node[iPoint]->SetCoord(Coord_Old);
      }
  }
  
  delete[] Coord;
}

bool CPhysicalGeometry::FindFace(unsigned long first_elem, unsigned long second_elem, unsigned short &face_first_elem,
                                 unsigned short &face_second_elem) {
  
  /*--- Find repeated nodes between two elements to identify the common face ---*/
  unsigned long iPoint = 0, jPoint = 0;
  unsigned short face_node, iFace, iNode, jNode, nNodesFace;
  vector<unsigned long> CommonPoints, PointFaceFirst, PointFaceSecond;
  vector<unsigned long>::iterator IterPoint;
  pair<vector <unsigned long>::iterator, vector <unsigned long>::iterator> mypair;
  bool face_first_found = false, face_second_found =false;
  
  if (first_elem == second_elem) return false;
  
  for (iNode = 0; iNode < elem[first_elem]->GetnNodes(); iNode++) {
    iPoint = elem[first_elem]->GetNode(iNode);
    for (jNode = 0; jNode < elem[second_elem]->GetnNodes(); jNode++) {
      jPoint = elem[second_elem]->GetNode(jNode);
      if (iPoint == jPoint) {
        CommonPoints.push_back(iPoint);
        break;
      }
    }
  }
  
  /*--- Sort point in face and check that the list is unique ---*/
  sort( CommonPoints.begin(), CommonPoints.end());
  IterPoint = unique( CommonPoints.begin(), CommonPoints.end());
  CommonPoints.resize( distance(CommonPoints.begin(), IterPoint) );
  
  /*--- In 2D, the two elements must share two points that make up
   an edge, as all "faces" are edges in 2D. In 3D, we need to find
   exactly 3 (tri) or 4 (quad) common points. Return immediately to
   avoid a memory issue due to vectors of different lengths below. ---*/
  
  if ((nDim == 2) && (CommonPoints.size() != 2)) return false;
  if ((nDim == 3) && ((CommonPoints.size() != 3) &&
                      (CommonPoints.size() != 4))) return false;
  
  /*--- Search the sequence in the first element ---*/
  for (iFace = 0; iFace < elem[first_elem]->GetnFaces(); iFace++) {
    nNodesFace = elem[first_elem]->GetnNodesFace(iFace);
    
    if (nNodesFace == CommonPoints.size()) {
    for (iNode = 0; iNode < nNodesFace; iNode++) {
      face_node = elem[first_elem]->GetFaces(iFace, iNode);
      PointFaceFirst.push_back(elem[first_elem]->GetNode(face_node));
    }
    
    /*--- Sort face_poin to perform comparison ---*/
    sort( PointFaceFirst.begin(), PointFaceFirst.end());
    
    /*--- List comparison ---*/
    mypair = mismatch (PointFaceFirst.begin(), PointFaceFirst.end(), CommonPoints.begin());
    if (mypair.first == PointFaceFirst.end()) {
      face_first_elem = iFace;
      face_first_found = true;
      break;
    }
    
    PointFaceFirst.erase (PointFaceFirst.begin(), PointFaceFirst.end());
  }
  }
  
  /*--- Search the secuence in the second element ---*/
  for (iFace = 0; iFace < elem[second_elem]->GetnFaces(); iFace++) {
    nNodesFace = elem[second_elem]->GetnNodesFace(iFace);
    
    if (nNodesFace == CommonPoints.size()) {
    for (iNode = 0; iNode < nNodesFace; iNode++) {
      face_node = elem[second_elem]->GetFaces(iFace, iNode);
      PointFaceSecond.push_back(elem[second_elem]->GetNode(face_node));
    }
    
    /*--- Sort face_poin to perform comparison ---*/
    sort( PointFaceSecond.begin(), PointFaceSecond.end());
    
    /*--- List comparison ---*/
    mypair = mismatch (PointFaceSecond.begin(), PointFaceSecond.end(), CommonPoints.begin());
    if (mypair.first == PointFaceSecond.end()) {
      face_second_elem = iFace;
      face_second_found = true;
      break;
    }
    
    PointFaceSecond.erase (PointFaceSecond.begin(), PointFaceSecond.end());
  }
  }
  
  if (face_first_found && face_second_found) return true;
  else return false;
  
}

void CPhysicalGeometry::SetTecPlot(char mesh_filename[MAX_STRING_SIZE], bool new_file) {
  
  unsigned long iElem, iPoint;
  unsigned short iDim;
  ofstream Tecplot_File;
  
  /*--- Open the tecplot file and write the header ---*/
  
  if (new_file) {
    Tecplot_File.open(mesh_filename, ios::out);
    Tecplot_File << "TITLE= \"Visualization of the volumetric grid\"" << endl;
    if (nDim == 2) Tecplot_File << "VARIABLES = \"x\",\"y\" " << endl;
    if (nDim == 3) Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\" " << endl;
  }
  else Tecplot_File.open(mesh_filename, ios::out | ios::app);
  
  Tecplot_File << "ZONE T= ";
  if (new_file) Tecplot_File << "\"Original grid\", C=BLACK, ";
  else Tecplot_File << "\"Deformed grid\", C=RED, ";
  Tecplot_File << "NODES= "<< nPoint <<", ELEMENTS= "<< nElem <<", DATAPACKING= POINT";
  if (nDim == 2) Tecplot_File << ", ZONETYPE= FEQUADRILATERAL"<< endl;
  if (nDim == 3) Tecplot_File << ", ZONETYPE= FEBRICK"<< endl;
  
  /*--- Adding coordinates ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++)
      Tecplot_File << scientific << node[iPoint]->GetCoord(iDim) << "\t";
    Tecplot_File << "\n";
  }
  
  /*--- Adding conectivity ---*/
  
  for (iElem = 0; iElem < nElem; iElem++) {
    if (elem[iElem]->GetVTK_Type() == TRIANGLE) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(2)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == QUADRILATERAL) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == TETRAHEDRON) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(2)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(3)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == HEXAHEDRON) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(5)+1 <<" "<<
      elem[iElem]->GetNode(6)+1 <<" "<< elem[iElem]->GetNode(7)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == PYRAMID) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(4)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(4)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == PRISM) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(1)+1 <<" "<< elem[iElem]->GetNode(2)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(4)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(5)+1 << endl;
    }
  }
  
  Tecplot_File.close();
}

void CPhysicalGeometry::SetBoundTecPlot(char mesh_filename[MAX_STRING_SIZE], bool new_file, CConfig *config) {
  
  ofstream Tecplot_File;
  unsigned long iPoint, Total_nElem_Bound, iElem, *PointSurface = NULL, nPointSurface = 0;
  unsigned short Coord_i, iMarker;
  
  /*--- It is important to do a renumbering to don't add points
   that do not belong to the surfaces ---*/
  
  PointSurface = new unsigned long[nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    if (node[iPoint]->GetBoundary()) {
      PointSurface[iPoint] = nPointSurface;
      nPointSurface++;
    }
  
  /*--- Compute the total number of elements ---*/
  
  Total_nElem_Bound = 0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_Plotting(iMarker) == YES) {
      Total_nElem_Bound += nElem_Bound[iMarker];
    }
  }
  
  /*--- Open the tecplot file and write the header ---*/
  
  if (new_file) {
    Tecplot_File.open(mesh_filename, ios::out);
    Tecplot_File << "TITLE= \"Visualization of the surface grid\"" << endl;
    if (nDim == 2) Tecplot_File << "VARIABLES = \"x\",\"y\" " << endl;
    if (nDim == 3) Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\" " << endl;
  }
  else Tecplot_File.open(mesh_filename, ios::out | ios::app);
  
  if (Total_nElem_Bound != 0) {
    
    /*--- Write the header of the file ---*/
    
    Tecplot_File << "ZONE T= ";
    if (new_file) Tecplot_File << "\"Original grid\", C=BLACK, ";
    else Tecplot_File << "\"Deformed grid\", C=RED, ";
    Tecplot_File << "NODES= "<< nPointSurface <<", ELEMENTS= "<< Total_nElem_Bound <<", DATAPACKING= POINT";
    if (nDim == 2) Tecplot_File << ", ZONETYPE= FELINESEG"<< endl;
    if (nDim == 3) Tecplot_File << ", ZONETYPE= FEQUADRILATERAL"<< endl;
    
    /*--- Only write the coordiantes of the points that are on the surfaces ---*/
    
    if (nDim == 3) {
      for (iPoint = 0; iPoint < nPoint; iPoint++)
        if (node[iPoint]->GetBoundary()) {
          for (Coord_i = 0; Coord_i < nDim-1; Coord_i++)
            Tecplot_File << node[iPoint]->GetCoord(Coord_i) << " ";
          Tecplot_File << node[iPoint]->GetCoord(nDim-1) << "\n";
        }
    }
    else {
      for (iPoint = 0; iPoint < nPoint; iPoint++)
        if (node[iPoint]->GetBoundary()) {
          for (Coord_i = 0; Coord_i < nDim; Coord_i++)
            Tecplot_File << node[iPoint]->GetCoord(Coord_i) << " ";
          Tecplot_File << "\n";
        }
    }
    
    /*--- Write the cells using the new numbering ---*/
    
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
      if (config->GetMarker_All_Plotting(iMarker) == YES)
        for (iElem = 0; iElem < nElem_Bound[iMarker]; iElem++) {
          if (nDim == 2) {
            Tecplot_File << PointSurface[bound[iMarker][iElem]->GetNode(0)]+1 << " "
            << PointSurface[bound[iMarker][iElem]->GetNode(1)]+1 << endl;
          }
          if (nDim == 3) {
            if (bound[iMarker][iElem]->GetnNodes() == 3) {
              Tecplot_File << PointSurface[bound[iMarker][iElem]->GetNode(0)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(1)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(2)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(2)]+1 << endl;
            }
            if (bound[iMarker][iElem]->GetnNodes() == 4) {
              Tecplot_File << PointSurface[bound[iMarker][iElem]->GetNode(0)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(1)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(2)]+1 << " "
              << PointSurface[bound[iMarker][iElem]->GetNode(3)]+1 << endl;
            }
          }
        }
  }
  else {
    
    /*--- No elements in the surface ---*/
    
    if (nDim == 2) {
      Tecplot_File << "ZONE NODES= 1, ELEMENTS= 1, DATAPACKING=POINT, ZONETYPE=FELINESEG"<< endl;
      Tecplot_File << "0.0 0.0"<< endl;
      Tecplot_File << "1 1"<< endl;
    }
    if (nDim == 3) {
      Tecplot_File << "ZONE NODES= 1, ELEMENTS= 1, DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
      Tecplot_File << "0.0 0.0 0.0"<< endl;
      Tecplot_File << "1 1 1 1"<< endl;
    }
  }
  
  /*--- Dealocate memory and close the file ---*/
  
  delete[] PointSurface;
  Tecplot_File.close();
  
}

void CPhysicalGeometry::SetColorGrid(CConfig *config) {
  
#ifdef HAVE_MPI
#ifdef HAVE_METIS
  
  unsigned long iPoint, iElem, iElem_Triangle, iElem_Tetrahedron, nElem_Triangle,
  nElem_Tetrahedron;
  idx_t ne = 0, nn, *elmnts = NULL, *epart = NULL, *npart = NULL, nparts, edgecut, *eptr;
  int rank, size;
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  if (size != SINGLE_ZONE)
    cout << endl <<"---------------------------- Grid partitioning --------------------------" << endl;
  
  unsigned short nDomain = size;
  
  nElem_Triangle = 0;
  nElem_Tetrahedron = 0;
  for (iElem = 0; iElem < GetnElem(); iElem++) {
    if (elem[iElem]->GetVTK_Type() == TRIANGLE)    nElem_Triangle = nElem_Triangle + 1;
    if (elem[iElem]->GetVTK_Type() == QUADRILATERAL)   nElem_Triangle = nElem_Triangle + 2;
    if (elem[iElem]->GetVTK_Type() == TETRAHEDRON) nElem_Tetrahedron = nElem_Tetrahedron + 1;
    if (elem[iElem]->GetVTK_Type() == HEXAHEDRON)  nElem_Tetrahedron = nElem_Tetrahedron + 5;
    if (elem[iElem]->GetVTK_Type() == PYRAMID)     nElem_Tetrahedron = nElem_Tetrahedron + 2;
    if (elem[iElem]->GetVTK_Type() == PRISM)       nElem_Tetrahedron = nElem_Tetrahedron + 3;
  }
  
  if (GetnDim() == 2) {
    ne = nElem_Triangle;
    elmnts = new idx_t [ne*3];
  }
  if (GetnDim() == 3) {
    ne = nElem_Tetrahedron;
    elmnts = new idx_t [ne*4];
  }
  
  nn = nPoint;
  nparts = nDomain;
  epart = new idx_t [ne];
  npart = new idx_t [nn];
  eptr  = new idx_t[ne+1];
  
  /*--- Initialize the color vector ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++)
    node[iPoint]->SetColor(0);
  
  if (nparts > 1) {
    
    iElem_Triangle = 0; iElem_Tetrahedron = 0;
    for (iElem = 0; iElem < GetnElem(); iElem++) {
      if (elem[iElem]->GetVTK_Type() == TRIANGLE) {
        elmnts[3*iElem_Triangle+0]= elem[iElem]->GetNode(0);
        elmnts[3*iElem_Triangle+1]= elem[iElem]->GetNode(1);
        elmnts[3*iElem_Triangle+2]= elem[iElem]->GetNode(2);
        eptr[iElem_Triangle] = 3*iElem_Triangle;
        iElem_Triangle++;
      }
      if (elem[iElem]->GetVTK_Type() == QUADRILATERAL) {
        elmnts[3*iElem_Triangle+0]= elem[iElem]->GetNode(0);
        elmnts[3*iElem_Triangle+1]= elem[iElem]->GetNode(1);
        elmnts[3*iElem_Triangle+2]= elem[iElem]->GetNode(2);
        eptr[iElem_Triangle] = 3*iElem_Triangle;
        iElem_Triangle++;
        elmnts[3*iElem_Triangle+0]= elem[iElem]->GetNode(0);
        elmnts[3*iElem_Triangle+1]= elem[iElem]->GetNode(2);
        elmnts[3*iElem_Triangle+2]= elem[iElem]->GetNode(3);
        eptr[iElem_Triangle] = 3*iElem_Triangle;
        iElem_Triangle++;
      }
      if (elem[iElem]->GetVTK_Type() == TETRAHEDRON) {
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(1);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(3);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
      }
      if (elem[iElem]->GetVTK_Type() == HEXAHEDRON) {
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(1);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(5);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(3);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(7);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(5);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(7);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(4);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(7);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(5);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(6);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(7);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(5);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
      }
      if (elem[iElem]->GetVTK_Type() == PYRAMID) {
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(1);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(4);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(3);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(4);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
      }
      if (elem[iElem]->GetVTK_Type() == PRISM) {
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(1);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(4);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(2);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(0);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(2);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(3);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(4);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
        elmnts[4*iElem_Tetrahedron+0]= elem[iElem]->GetNode(3);
        elmnts[4*iElem_Tetrahedron+1]= elem[iElem]->GetNode(4);
        elmnts[4*iElem_Tetrahedron+2]= elem[iElem]->GetNode(5);
        elmnts[4*iElem_Tetrahedron+3]= elem[iElem]->GetNode(2);
        eptr[iElem_Tetrahedron] = 4*iElem_Tetrahedron;
        iElem_Tetrahedron++;
      }
    }
    
    /*--- Add final value to element pointer array ---*/
    
    if (GetnDim() == 2) eptr[ne] = 3*ne;
    else eptr[ne] = 4*ne;
    
    METIS_PartMeshNodal(&ne, &nn, eptr, elmnts, NULL, NULL, &nparts, NULL, NULL, &edgecut, epart, npart);
    
    cout << "Finished partitioning using METIS. ("  << edgecut << " edge cuts)." << endl;
    
    for (iPoint = 0; iPoint < nPoint; iPoint++)
      node[iPoint]->SetColor(npart[iPoint]);
  }
  
  delete[] epart;
  delete[] npart;
  delete[] elmnts;
  delete[] eptr;
  
#endif
  
#endif
  
}

void CPhysicalGeometry::SetColorGrid_Parallel(CConfig *config) {
  
  /*--- Initialize the color vector ---*/
  
  for (unsigned long iPoint = 0; iPoint < nPoint; iPoint++)
    node[iPoint]->SetColor(0);
  
  /*--- This routine should only ever be called if we have parallel support
   with MPI and have the ParMETIS library compiled and linked. ---*/
  
#ifdef HAVE_MPI
#ifdef HAVE_PARMETIS
  
  unsigned long iPoint;
  int rank, size;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  /*--- Only call ParMETIS if we have more than one rank to avoid errors ---*/
  
  if (size > SINGLE_NODE) {
    
    /*--- Create some structures that ParMETIS needs for partitioning. ---*/
    
    idx_t numflag, nparts, edgecut, wgtflag, ncon;
    
    idx_t *vtxdist = new idx_t[size+1];
    idx_t *part    = new idx_t[nPoint];
    
    real_t ubvec;
    real_t *tpwgts = new real_t[size];
    
    /*--- Some recommended defaults for the various ParMETIS options. ---*/
    
    wgtflag = 0;
    numflag = 0;
    ncon    = 1;
    ubvec   = 1.05;
    nparts  = (idx_t)size;
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    options[1] = 0;
    
    /*--- Fill the necessary ParMETIS data arrays. Note that xadj_size and
     adjacency_size are class data members that have been defined and set
     earlier in the partitioning process. ---*/
    
    for (int i = 0; i < size; i++) {
      tpwgts[i] = 1.0/((real_t)size);
    }
    
    vtxdist[0] = 0;
    for (int i = 0; i < size; i++) {
      vtxdist[i+1] = (idx_t)ending_node[i];
    }
    
    /*--- Calling ParMETIS ---*/
    if (rank == MASTER_NODE) cout << "Calling ParMETIS..." << endl;
    ParMETIS_V3_PartKway(vtxdist,xadj, adjacency, NULL, NULL, &wgtflag,
                         &numflag, &ncon, &nparts, tpwgts, &ubvec, options,
                         &edgecut, part, &comm);
    if (rank == MASTER_NODE) {
      cout << "Finished partitioning using ParMETIS (";
      cout << edgecut << " edge cuts)." << endl;
    }
    
    /*--- Store the results of the partitioning (note that this is local
     since each processor is calling ParMETIS in parallel and storing the
     results for its initial piece of the grid. ---*/
    
    for (iPoint = 0; iPoint < nPoint; iPoint++) {
      node[iPoint]->SetColor(part[iPoint]);
    }
    
    /*--- Free all memory needed for the ParMETIS structures ---*/
    
    delete [] vtxdist;
    delete [] part;
    delete [] tpwgts;
    
  }
  
  /*--- Delete the memory from the geometry class that carried the
   adjacency structure. ---*/
  
  delete [] xadj;
  delete [] adjacency;
  
#endif
#endif
  
}

void CPhysicalGeometry::GetQualityStatistics(su2double *statistics) {
  unsigned long jPoint, Point_2, Point_3, iElem;
  su2double *Coord_j, *Coord_2, *Coord_3;
  unsigned short iDim;
  
  statistics[0] = 1e06;
  statistics[1] = 0;
  
  /*--- Loop interior edges ---*/
  for (iElem = 0; iElem < this->GetnElem(); iElem++) {
    
    if ((this->GetnDim() == 2) && (elem[iElem]->GetVTK_Type() == TRIANGLE)) {
      
      jPoint = elem[iElem]->GetNode(0); Coord_j = node[jPoint]->GetCoord();
      Point_2 = elem[iElem]->GetNode(1); Coord_2 = node[Point_2]->GetCoord();
      Point_3 = elem[iElem]->GetNode(2); Coord_3 = node[Point_3]->GetCoord();
      
      /*--- Compute sides of the triangle ---*/
      su2double a = 0, b = 0, c = 0;
      for (iDim = 0; iDim < nDim; iDim++) {
        a += (Coord_2[iDim]-Coord_j[iDim])*(Coord_2[iDim]-Coord_j[iDim]);
        b += (Coord_3[iDim]-Coord_j[iDim])*(Coord_3[iDim]-Coord_j[iDim]);
        c += (Coord_3[iDim]-Coord_2[iDim])*(Coord_3[iDim]-Coord_2[iDim]);
      }
      a = sqrt(a); b = sqrt(b); c = sqrt(c);
      
      /*--- Compute semiperimeter (s) and area ---*/
      su2double s = 0.5*(a + b + c);
      su2double Area = sqrt(s*(s-a)*(s-b)*(s-c));
      
      /*--- Compute radius of the circumcircle (R) and of the incircle (r) ---*/
      su2double R = (a*b*c) / (4.0*Area);
      su2double r = Area / s;
      su2double roR = r / R;
      
      /*--- Update statistics ---*/
      if (roR < statistics[0])
        statistics[0] = roR;
      statistics[1] += roR;
      
    }
  }
  statistics[1] /= this->GetnElem();
  
}

void CPhysicalGeometry::SetRotationalVelocity(CConfig *config, unsigned short val_iZone, bool print) {
  
  unsigned long iPoint;
  su2double RotVel[3], Distance[3], *Coord, Center[3], Omega[3], L_Ref;
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  /*--- Center of rotation & angular velocity vector from config ---*/
  
  Center[0] = config->GetMotion_Origin_X(val_iZone);
  Center[1] = config->GetMotion_Origin_Y(val_iZone);
  Center[2] = config->GetMotion_Origin_Z(val_iZone);
  Omega[0]  = config->GetRotation_Rate_X(val_iZone)/config->GetOmega_Ref();
  Omega[1]  = config->GetRotation_Rate_Y(val_iZone)/config->GetOmega_Ref();
  Omega[2]  = config->GetRotation_Rate_Z(val_iZone)/config->GetOmega_Ref();
  L_Ref     = config->GetLength_Ref();
  
  /*--- Print some information to the console ---*/
  
  if (rank == MASTER_NODE && print) {
    cout << " Rotational origin (x, y, z): ( " << Center[0] << ", " << Center[1];
    cout << ", " << Center[2] << " )" << endl;
    cout << " Angular velocity about x, y, z axes: ( " << Omega[0] << ", ";
    cout << Omega[1] << ", " << Omega[2] << " ) rad/s" << endl;
  }
  
  /*--- Loop over all nodes and set the rotational velocity ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    
    /*--- Get the coordinates of the current node ---*/
    
    Coord = node[iPoint]->GetCoord();
    
    /*--- Calculate the non-dim. distance from the rotation center ---*/
    
    Distance[0] = (Coord[0]-Center[0])/L_Ref;
    Distance[1] = (Coord[1]-Center[1])/L_Ref;
    Distance[2] = 0.0;
    if (nDim == 3)
    	Distance[2] = (Coord[2]-Center[2])/L_Ref;
    
    /*--- Calculate the angular velocity as omega X r ---*/
    
    RotVel[0] = Omega[1]*(Distance[2]) - Omega[2]*(Distance[1]);
    RotVel[1] = Omega[2]*(Distance[0]) - Omega[0]*(Distance[2]);
    RotVel[2] = 0.0;
    if (nDim == 3)
    	RotVel[2] = Omega[0]*(Distance[1]) - Omega[1]*(Distance[0]);
    
    /*--- Store the grid velocity at this node ---*/
    
    node[iPoint]->SetGridVel(RotVel);
    
  }
  
}

void CPhysicalGeometry::SetShroudVelocity(CConfig *config) {

  unsigned long iPoint, iVertex;
  unsigned short iMarker, iMarkerShroud;
  su2double RotVel[3];

  RotVel[0] = 0.0;
  RotVel[1] = 0.0;
  RotVel[2] = 0.0;

  /*--- Loop over all vertex in the shroud marker and set the rotational velocity to 0.0 ---*/
  for (iMarker = 0; iMarker < nMarker; iMarker++){
    for(iMarkerShroud=0; iMarkerShroud < config->GetnMarker_Shroud(); iMarkerShroud++){
      if(config->GetMarker_Shroud(iMarkerShroud) == config->GetMarker_All_TagBound(iMarker)){
        for (iVertex = 0; iVertex  < nVertex[iMarker]; iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          node[iPoint]->SetGridVel(RotVel);
        }
      }
    }
  }
}

void CPhysicalGeometry::SetTranslationalVelocity(CConfig *config, unsigned short val_iZone, bool print) {
  
  unsigned short iDim;
  unsigned long iPoint;
  su2double xDot[3] = {0.0,0.0,0.0};
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  /*--- Get the translational velocity vector from config ---*/
  
  xDot[0] = config->GetTranslation_Rate_X(val_iZone)/config->GetVelocity_Ref();
  xDot[1] = config->GetTranslation_Rate_Y(val_iZone)/config->GetVelocity_Ref();
  xDot[2] = config->GetTranslation_Rate_Z(val_iZone)/config->GetVelocity_Ref();
  
  /*--- Print some information to the console ---*/
  
  if (rank == MASTER_NODE && print) {
    cout << " Non-dim. translational velocity: (" << xDot[0] << ", " << xDot[1];
    cout << ", " << xDot[2] << ")." << endl;
  }
  
  /*--- Loop over all nodes and set the translational velocity ---*/
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    
    /*--- Store the grid velocity at this node ---*/
    
    for (iDim = 0; iDim < nDim; iDim++) {
      node[iPoint]->SetGridVel(iDim,xDot[iDim]);
    }
  
  }
  
}

void CPhysicalGeometry::SetGridVelocity(CConfig *config, unsigned long iter) {
  
  /*--- Local variables ---*/
  
  su2double *Coord_nP1 = NULL, *Coord_n = NULL, *Coord_nM1 = NULL;
  su2double TimeStep, GridVel = 0.0;
  unsigned long iPoint;
  unsigned short iDim;
  
  /*--- Compute the velocity of each node in the volume mesh ---*/
  
  for (iPoint = 0; iPoint < GetnPoint(); iPoint++) {
    
    /*--- Coordinates of the current point at n+1, n, & n-1 time levels ---*/
    
    Coord_nM1 = node[iPoint]->GetCoord_n1();
    Coord_n   = node[iPoint]->GetCoord_n();
    Coord_nP1 = node[iPoint]->GetCoord();

    /*--- Unsteady time step ---*/
    
    TimeStep = config->GetDelta_UnstTimeND();
    
    /*--- Compute mesh velocity with 1st or 2nd-order approximation ---*/
    
    for (iDim = 0; iDim < nDim; iDim++) {
      if (config->GetUnsteady_Simulation() == DT_STEPPING_1ST)
        GridVel = ( Coord_nP1[iDim] - Coord_n[iDim] ) / TimeStep;
      if (config->GetUnsteady_Simulation() == DT_STEPPING_2ND)
        GridVel = ( 3.0*Coord_nP1[iDim] - 4.0*Coord_n[iDim]
                   + 1.0*Coord_nM1[iDim] ) / (2.0*TimeStep);
      
      /*--- Store grid velocity for this point ---*/
      
      node[iPoint]->SetGridVel(iDim, GridVel);
    }
  }
  
}

void CPhysicalGeometry::Set_MPI_Coord(CConfig *config) {
  
  unsigned short iDim, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi, *Buffer_Receive_Coord = NULL, *Buffer_Send_Coord = NULL, *Coord = NULL, *newCoord = NULL;
  su2double *translation;
  newCoord = new su2double[nDim];
  
#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {
      
      MarkerS = iMarker;  MarkerR = iMarker+1;
      
#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = nVertex[MarkerS];  nVertexR = nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nDim;        nBufferR_Vector = nVertexR*nDim;
      
      /*--- Allocate Receive and send buffers  ---*/
      
      Buffer_Receive_Coord = new su2double [nBufferR_Vector];
      Buffer_Send_Coord = new su2double[nBufferS_Vector];
      
      /*--- Copy the coordinates that should be sended ---*/
      
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = vertex[MarkerS][iVertex]->GetNode();
        Coord = node[iPoint]->GetCoord();
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Send_Coord[iDim*nVertexS+iVertex] = Coord[iDim];
      }
      
#ifdef HAVE_MPI
      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_Coord, nBufferS_Vector, MPI_DOUBLE, send_to,0,
                   Buffer_Receive_Coord, nBufferR_Vector, MPI_DOUBLE, receive_from,0, MPI_COMM_WORLD, &status);
#else
      
      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Receive_Coord[iDim*nVertexR+iVertex] = Buffer_Send_Coord[iDim*nVertexR+iVertex];
      }
      
#endif
      
      /*--- Deallocate send buffer ---*/
      
      delete [] Buffer_Send_Coord;
      
      /*--- Do the coordinate transformation ---*/
      
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        
        /*--- Find point and its type of transformation ---*/
        
        iPoint = vertex[MarkerR][iVertex]->GetNode();
        iPeriodic_Index = vertex[MarkerR][iVertex]->GetRotation_Type();
        
        /*--- Retrieve the supplied periodic information. ---*/
        
        angles = config->GetPeriodicRotation(iPeriodic_Index);
        translation = config->GetPeriodicTranslate(iPeriodic_Index);
        
        /*--- Store angles separately for clarity. ---*/
        
        theta    = angles[0];   phi    = angles[1];     psi    = angles[2];
        cosTheta = cos(theta);  cosPhi = cos(phi);      cosPsi = cos(psi);
        sinTheta = sin(theta);  sinPhi = sin(phi);      sinPsi = sin(psi);
        
        /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis,
         then z-axis. Note that this is the transpose of the matrix
         used during the preprocessing stage. ---*/
        
        rotMatrix[0][0] = cosPhi*cosPsi;    rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;     rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
        rotMatrix[0][1] = cosPhi*sinPsi;    rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;     rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
        rotMatrix[0][2] = -sinPhi;          rotMatrix[1][2] = sinTheta*cosPhi;                              rotMatrix[2][2] = cosTheta*cosPhi;
        
        /*--- Copy coordinates before performing transformation. ---*/
        
        for (iDim = 0; iDim < nDim; iDim++)
          newCoord[iDim] = Buffer_Receive_Coord[iDim*nVertexR+iVertex];
        
        /*--- Rotate the coordinates. ---*/
        
        if (nDim == 2) {
          newCoord[0] = (rotMatrix[0][0]*Buffer_Receive_Coord[0*nVertexR+iVertex] +
                         rotMatrix[0][1]*Buffer_Receive_Coord[1*nVertexR+iVertex]) - translation[0];
          newCoord[1] = (rotMatrix[1][0]*Buffer_Receive_Coord[0*nVertexR+iVertex] +
                         rotMatrix[1][1]*Buffer_Receive_Coord[1*nVertexR+iVertex]) - translation[1];
        }
        else {
          newCoord[0] = (rotMatrix[0][0]*Buffer_Receive_Coord[0*nVertexR+iVertex] +
                         rotMatrix[0][1]*Buffer_Receive_Coord[1*nVertexR+iVertex] +
                         rotMatrix[0][2]*Buffer_Receive_Coord[2*nVertexR+iVertex]);
          newCoord[1] = (rotMatrix[1][0]*Buffer_Receive_Coord[0*nVertexR+iVertex] +
                         rotMatrix[1][1]*Buffer_Receive_Coord[1*nVertexR+iVertex] +
                         rotMatrix[1][2]*Buffer_Receive_Coord[2*nVertexR+iVertex]);
          newCoord[2] = (rotMatrix[2][0]*Buffer_Receive_Coord[0*nVertexR+iVertex] +
                         rotMatrix[2][1]*Buffer_Receive_Coord[1*nVertexR+iVertex] +
                         rotMatrix[2][2]*Buffer_Receive_Coord[2*nVertexR+iVertex]);
        }
        
        /*--- Copy transformed coordinates back into buffer. ---*/
        
        for (iDim = 0; iDim < nDim; iDim++)
          node[iPoint]->SetCoord(iDim, newCoord[iDim]);
        
      }
      
      /*--- Deallocate receive buffer. ---*/
      
      delete [] Buffer_Receive_Coord;
      
    }
    
  }
  
  delete [] newCoord;
  
}

void CPhysicalGeometry::Set_MPI_GridVel(CConfig *config) {
  
  unsigned short iDim, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi, *Buffer_Receive_GridVel = NULL, *Buffer_Send_GridVel = NULL, *GridVel = NULL, *newGridVel = NULL;
  
  newGridVel = new su2double[nDim];
  
#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {
      
      MarkerS = iMarker;  MarkerR = iMarker+1;
     
#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif
   
      nVertexS = nVertex[MarkerS];  nVertexR = nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nDim;        nBufferR_Vector = nVertexR*nDim;
      
      /*--- Allocate Receive and send buffers  ---*/
      
      Buffer_Receive_GridVel = new su2double [nBufferR_Vector];
      Buffer_Send_GridVel = new su2double[nBufferS_Vector];
      
      /*--- Copy the grid velocity that should be sended ---*/
      
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = vertex[MarkerS][iVertex]->GetNode();
        GridVel = node[iPoint]->GetGridVel();
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Send_GridVel[iDim*nVertexS+iVertex] = GridVel[iDim];
      }
      
#ifdef HAVE_MPI
      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_GridVel, nBufferS_Vector, MPI_DOUBLE, send_to,0,
                   Buffer_Receive_GridVel, nBufferR_Vector, MPI_DOUBLE, receive_from,0, MPI_COMM_WORLD, &status);
#else
      
      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Receive_GridVel[iDim*nVertexR+iVertex] = Buffer_Send_GridVel[iDim*nVertexR+iVertex];
      }
      
#endif
      
      /*--- Deallocate send buffer ---*/
      
      delete [] Buffer_Send_GridVel;
      
      /*--- Do the coordinate transformation ---*/
      
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        
        /*--- Find point and its type of transformation ---*/
        
        iPoint = vertex[MarkerR][iVertex]->GetNode();
        iPeriodic_Index = vertex[MarkerR][iVertex]->GetRotation_Type();
        
        /*--- Retrieve the supplied periodic information. ---*/
        
        angles = config->GetPeriodicRotation(iPeriodic_Index);
        
        /*--- Store angles separately for clarity. ---*/
        theta    = angles[0];   phi    = angles[1];     psi    = angles[2];
        cosTheta = cos(theta);  cosPhi = cos(phi);      cosPsi = cos(psi);
        sinTheta = sin(theta);  sinPhi = sin(phi);      sinPsi = sin(psi);
        
        /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis,
         then z-axis. Note that this is the transpose of the matrix
         used during the preprocessing stage. ---*/
        
        rotMatrix[0][0] = cosPhi*cosPsi;    rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;     rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
        rotMatrix[0][1] = cosPhi*sinPsi;    rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;     rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
        rotMatrix[0][2] = -sinPhi;          rotMatrix[1][2] = sinTheta*cosPhi;                              rotMatrix[2][2] = cosTheta*cosPhi;
        
        /*--- Copy grid velocity before performing transformation. ---*/
        
        for (iDim = 0; iDim < nDim; iDim++)
          newGridVel[iDim] = Buffer_Receive_GridVel[iDim*nVertexR+iVertex];
        
        if (nDim == 2) {
          newGridVel[0] = (rotMatrix[0][0]*Buffer_Receive_GridVel[0*nVertexR+iVertex] +
                           rotMatrix[0][1]*Buffer_Receive_GridVel[1*nVertexR+iVertex]);
          newGridVel[1] = (rotMatrix[1][0]*Buffer_Receive_GridVel[0*nVertexR+iVertex] +
                           rotMatrix[1][1]*Buffer_Receive_GridVel[1*nVertexR+iVertex]);
        }
        else {
          newGridVel[0] = (rotMatrix[0][0]*Buffer_Receive_GridVel[0*nVertexR+iVertex] +
                           rotMatrix[0][1]*Buffer_Receive_GridVel[1*nVertexR+iVertex] +
                           rotMatrix[0][2]*Buffer_Receive_GridVel[2*nVertexR+iVertex]);
          newGridVel[1] = (rotMatrix[1][0]*Buffer_Receive_GridVel[0*nVertexR+iVertex] +
                           rotMatrix[1][1]*Buffer_Receive_GridVel[1*nVertexR+iVertex] +
                           rotMatrix[1][2]*Buffer_Receive_GridVel[2*nVertexR+iVertex]);
          newGridVel[2] = (rotMatrix[2][0]*Buffer_Receive_GridVel[0*nVertexR+iVertex] +
                           rotMatrix[2][1]*Buffer_Receive_GridVel[1*nVertexR+iVertex] +
                           rotMatrix[2][2]*Buffer_Receive_GridVel[2*nVertexR+iVertex]);
        }
        
        /*--- Copy transformed grid velocity back into buffer. ---*/
        
        for (iDim = 0; iDim < nDim; iDim++)
          node[iPoint]->SetGridVel(iDim, newGridVel[iDim]);
        
      }
      
      /*--- Deallocate receive buffer ---*/
      
      delete [] Buffer_Receive_GridVel;
      
    }
    
  }
  
  delete [] newGridVel;
  
}

void CPhysicalGeometry::Set_MPI_OldCoord(CConfig *config) {

  unsigned short iDim, iMarker, iPeriodic_Index, MarkerS, MarkerR;
  unsigned long iVertex, iPoint, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector;
  su2double rotMatrix[3][3], *angles, theta, cosTheta, sinTheta, phi, cosPhi, sinPhi, psi, cosPsi, sinPsi;

  su2double *Buffer_Receive_Coord_n = NULL, *Buffer_Send_Coord_n = NULL, *Coord_n = NULL, *newCoord_n = NULL;

  newCoord_n = new su2double[nDim];

#ifdef HAVE_MPI
  int send_to, receive_from;
  MPI_Status status;
#endif

  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {

      MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = nVertex[MarkerS];  nVertexR = nVertex[MarkerR];
      nBufferS_Vector = nVertexS*nDim;        nBufferR_Vector = nVertexR*nDim;

      /*--- Allocate Receive and send buffers  ---*/

      Buffer_Receive_Coord_n = new su2double [nBufferR_Vector];
      Buffer_Send_Coord_n = new su2double[nBufferS_Vector];

      /*--- Copy the coordinates that should be sended ---*/

      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = vertex[MarkerS][iVertex]->GetNode();
        Coord_n = node[iPoint]->GetCoord_n();
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Send_Coord_n[iDim*nVertexS+iVertex] = Coord_n[iDim];
      }

#ifdef HAVE_MPI
      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_Coord_n, nBufferS_Vector, MPI_DOUBLE, send_to,0,
                   Buffer_Receive_Coord_n, nBufferR_Vector, MPI_DOUBLE, receive_from,0, MPI_COMM_WORLD, &status);
#else

      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        for (iDim = 0; iDim < nDim; iDim++)
          Buffer_Receive_Coord_n[iDim*nVertexR+iVertex] = Buffer_Send_Coord_n[iDim*nVertexR+iVertex];
      }

#endif

      /*--- Deallocate send buffer ---*/

      delete [] Buffer_Send_Coord_n;

      /*--- Do the coordinate transformation ---*/

      for (iVertex = 0; iVertex < nVertexR; iVertex++) {

        /*--- Find point and its type of transformation ---*/

        iPoint = vertex[MarkerR][iVertex]->GetNode();
        iPeriodic_Index = vertex[MarkerR][iVertex]->GetRotation_Type();

        /*--- Retrieve the supplied periodic information. ---*/

        angles = config->GetPeriodicRotation(iPeriodic_Index);

        /*--- Store angles separately for clarity. ---*/

        theta    = angles[0];   phi    = angles[1];     psi    = angles[2];
        cosTheta = cos(theta);  cosPhi = cos(phi);      cosPsi = cos(psi);
        sinTheta = sin(theta);  sinPhi = sin(phi);      sinPsi = sin(psi);

        /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis,
         then z-axis. Note that this is the transpose of the matrix
         used during the preprocessing stage. ---*/

        rotMatrix[0][0] = cosPhi*cosPsi;    rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;     rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
        rotMatrix[0][1] = cosPhi*sinPsi;    rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;     rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
        rotMatrix[0][2] = -sinPhi;          rotMatrix[1][2] = sinTheta*cosPhi;                              rotMatrix[2][2] = cosTheta*cosPhi;

        /*--- Copy coordinates before performing transformation. ---*/

        for (iDim = 0; iDim < nDim; iDim++)
          newCoord_n[iDim] = Buffer_Receive_Coord_n[iDim*nVertexR+iVertex];

        /*--- Rotate the coordinates. ---*/

        if (nDim == 2) {
          newCoord_n[0] = (rotMatrix[0][0]*Buffer_Receive_Coord_n[0*nVertexR+iVertex] +
                         rotMatrix[0][1]*Buffer_Receive_Coord_n[1*nVertexR+iVertex]);
          newCoord_n[1] = (rotMatrix[1][0]*Buffer_Receive_Coord_n[0*nVertexR+iVertex] +
                         rotMatrix[1][1]*Buffer_Receive_Coord_n[1*nVertexR+iVertex]);
        }
        else {
          newCoord_n[0] = (rotMatrix[0][0]*Buffer_Receive_Coord_n[0*nVertexR+iVertex] +
                         rotMatrix[0][1]*Buffer_Receive_Coord_n[1*nVertexR+iVertex] +
                         rotMatrix[0][2]*Buffer_Receive_Coord_n[2*nVertexR+iVertex]);
          newCoord_n[1] = (rotMatrix[1][0]*Buffer_Receive_Coord_n[0*nVertexR+iVertex] +
                         rotMatrix[1][1]*Buffer_Receive_Coord_n[1*nVertexR+iVertex] +
                         rotMatrix[1][2]*Buffer_Receive_Coord_n[2*nVertexR+iVertex]);
          newCoord_n[2] = (rotMatrix[2][0]*Buffer_Receive_Coord_n[0*nVertexR+iVertex] +
                         rotMatrix[2][1]*Buffer_Receive_Coord_n[1*nVertexR+iVertex] +
                         rotMatrix[2][2]*Buffer_Receive_Coord_n[2*nVertexR+iVertex]);
        }

        /*--- Copy transformed coordinates back into buffer. ---*/

        node[iPoint]->SetCoord_n(newCoord_n);

      }

      /*--- Deallocate receive buffer. ---*/

      delete [] Buffer_Receive_Coord_n;

    }

  }

  delete [] newCoord_n;

  /*--------------------------------------------------------------------------------------------------*/
  /*--- We repeat the process for the coordinate n-1, in the case that the simulation is 2nd order ---*/
  /*--------------------------------------------------------------------------------------------------*/

  if (config->GetUnsteady_Simulation() == DT_STEPPING_2ND) {

	  su2double *Buffer_Receive_Coord_n1 = NULL, *Buffer_Send_Coord_n1 = NULL, *Coord_n1 = NULL, *newCoord_n1 = NULL;
	  newCoord_n1 = new su2double[nDim];

	  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {

		  if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
				  (config->GetMarker_All_SendRecv(iMarker) > 0)) {

			  MarkerS = iMarker;  MarkerR = iMarker+1;

#ifdef HAVE_MPI
			  send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
			  receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

			  nVertexS = nVertex[MarkerS];  nVertexR = nVertex[MarkerR];
			  nBufferS_Vector = nVertexS*nDim;        nBufferR_Vector = nVertexR*nDim;

			  /*--- Allocate Receive and send buffers  ---*/

			  Buffer_Receive_Coord_n1 = new su2double [nBufferR_Vector];
			  Buffer_Send_Coord_n1 = new su2double[nBufferS_Vector];

			  /*--- Copy the coordinates that should be sended ---*/

			  for (iVertex = 0; iVertex < nVertexS; iVertex++) {
				  iPoint = vertex[MarkerS][iVertex]->GetNode();
				  Coord_n1 = node[iPoint]->GetCoord_n1();
				  for (iDim = 0; iDim < nDim; iDim++)
					  Buffer_Send_Coord_n1[iDim*nVertexS+iVertex] = Coord_n1[iDim];
			  }

#ifdef HAVE_MPI
			  /*--- Send/Receive information using Sendrecv ---*/
			  SU2_MPI::Sendrecv(Buffer_Send_Coord_n1, nBufferS_Vector, MPI_DOUBLE, send_to,0,
					  Buffer_Receive_Coord_n1, nBufferR_Vector, MPI_DOUBLE, receive_from,0, MPI_COMM_WORLD, &status);
#else

			  /*--- Receive information without MPI ---*/
			  for (iVertex = 0; iVertex < nVertexR; iVertex++) {
				  for (iDim = 0; iDim < nDim; iDim++)
					  Buffer_Receive_Coord_n1[iDim*nVertexR+iVertex] = Buffer_Send_Coord_n1[iDim*nVertexR+iVertex];
			  }

#endif

			  /*--- Deallocate send buffer ---*/

			  delete [] Buffer_Send_Coord_n1;

			  /*--- Do the coordinate transformation ---*/

			  for (iVertex = 0; iVertex < nVertexR; iVertex++) {

				  /*--- Find point and its type of transformation ---*/

				  iPoint = vertex[MarkerR][iVertex]->GetNode();
				  iPeriodic_Index = vertex[MarkerR][iVertex]->GetRotation_Type();

				  /*--- Retrieve the supplied periodic information. ---*/

				  angles = config->GetPeriodicRotation(iPeriodic_Index);

				  /*--- Store angles separately for clarity. ---*/

				  theta    = angles[0];   phi    = angles[1];     psi    = angles[2];
				  cosTheta = cos(theta);  cosPhi = cos(phi);      cosPsi = cos(psi);
				  sinTheta = sin(theta);  sinPhi = sin(phi);      sinPsi = sin(psi);

				  /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis,
         then z-axis. Note that this is the transpose of the matrix
         used during the preprocessing stage. ---*/

				  rotMatrix[0][0] = cosPhi*cosPsi;    rotMatrix[1][0] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;     rotMatrix[2][0] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
				  rotMatrix[0][1] = cosPhi*sinPsi;    rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;     rotMatrix[2][1] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
				  rotMatrix[0][2] = -sinPhi;          rotMatrix[1][2] = sinTheta*cosPhi;                              rotMatrix[2][2] = cosTheta*cosPhi;

				  /*--- Copy coordinates before performing transformation. ---*/

				  for (iDim = 0; iDim < nDim; iDim++)
					  newCoord_n1[iDim] = Buffer_Receive_Coord_n1[iDim*nVertexR+iVertex];

				  /*--- Rotate the coordinates. ---*/

				  if (nDim == 2) {
					  newCoord_n1[0] = (rotMatrix[0][0]*Buffer_Receive_Coord_n1[0*nVertexR+iVertex] +
							  rotMatrix[0][1]*Buffer_Receive_Coord_n1[1*nVertexR+iVertex]);
					  newCoord_n1[1] = (rotMatrix[1][0]*Buffer_Receive_Coord_n1[0*nVertexR+iVertex] +
							  rotMatrix[1][1]*Buffer_Receive_Coord_n1[1*nVertexR+iVertex]);
				  }
				  else {
					  newCoord_n1[0] = (rotMatrix[0][0]*Buffer_Receive_Coord_n1[0*nVertexR+iVertex] +
							  rotMatrix[0][1]*Buffer_Receive_Coord_n1[1*nVertexR+iVertex] +
							  rotMatrix[0][2]*Buffer_Receive_Coord_n1[2*nVertexR+iVertex]);
					  newCoord_n1[1] = (rotMatrix[1][0]*Buffer_Receive_Coord_n1[0*nVertexR+iVertex] +
							  rotMatrix[1][1]*Buffer_Receive_Coord_n1[1*nVertexR+iVertex] +
							  rotMatrix[1][2]*Buffer_Receive_Coord_n1[2*nVertexR+iVertex]);
					  newCoord_n1[2] = (rotMatrix[2][0]*Buffer_Receive_Coord_n1[0*nVertexR+iVertex] +
							  rotMatrix[2][1]*Buffer_Receive_Coord_n1[1*nVertexR+iVertex] +
							  rotMatrix[2][2]*Buffer_Receive_Coord_n1[2*nVertexR+iVertex]);
				  }

				  /*--- Copy transformed coordinates back into buffer. ---*/

				  node[iPoint]->SetCoord_n1(newCoord_n1);

			  }

			  /*--- Deallocate receive buffer. ---*/

			  delete [] Buffer_Receive_Coord_n1;

		  }

	  }

	  delete [] newCoord_n1;

  }

  /*--------------------------------------------------------------------------------------------------*/

}

void CPhysicalGeometry::SetPeriodicBoundary(CConfig *config) {
  
  unsigned short iMarker, jMarker, kMarker = 0, iPeriodic, iDim, nPeriodic = 0, VTK_Type;
  unsigned long iNode, iIndex, iVertex, iPoint, iElem, kElem;
  unsigned long jElem, kPoint = 0, jVertex = 0, jPoint = 0, pPoint = 0, nPointPeriodic, newNodes[4] = {0,0,0,0};
  vector<unsigned long>::iterator IterElem, IterPoint[MAX_NUMBER_PERIODIC][2];
  su2double *center, *angles, rotMatrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}},
  translation[3], *trans, theta, phi, psi, cosTheta, sinTheta, cosPhi, sinPhi, cosPsi, sinPsi,
  dx, dy, dz, rotCoord[3], epsilon = 1e-10, mindist = 1e6, *Coord_i, *Coord_j, dist = 0.0;
  bool isBadMatch = false;

  /*--- Check this dimensionalization ---*/

  vector<unsigned long> OldBoundaryElems[100];
  vector<unsigned long>::iterator IterNewElem[100];

  /*--- We only create the mirror structure for the second boundary ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY) {
      /*--- Evaluate the number of periodic boundary conditions ---*/
      nPeriodic++;
    }
  }
  bool *CreateMirror = new bool[nPeriodic+1];
  CreateMirror[0] = false;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (iPeriodic <= nPeriodic/2) CreateMirror[iPeriodic] = false;
    else CreateMirror[iPeriodic] = true;
  }
  
  /*--- Send an initial message to the console. ---*/
  cout << "Setting the periodic boundary conditions." << endl;
  
  /*--- Loop through each marker to find any periodic boundaries. ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY) {
      
      /*--- Get marker index of the periodic donor boundary. ---*/
      jMarker = config->GetMarker_Periodic_Donor(config->GetMarker_All_TagBound(iMarker));
      
      /*--- Write some info to the console. ---*/
      cout << "Checking " << config->GetMarker_All_TagBound(iMarker);
      cout << " boundary against periodic donor, " << config->GetMarker_All_TagBound(jMarker) << ". ";
      
      /*--- Retrieve the supplied periodic information. ---*/
      center = config->GetPeriodicRotCenter(config->GetMarker_All_TagBound(iMarker));
      angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(iMarker));
      trans  = config->GetPeriodicTranslation(config->GetMarker_All_TagBound(iMarker));
      
      /*--- Store (center+trans) as it is constant and will be added on. ---*/
      translation[0] = center[0] + trans[0];
      translation[1] = center[1] + trans[1];
      translation[2] = center[2] + trans[2];
      
      /*--- Store angles separately for clarity. Compute sines/cosines. ---*/
      theta = angles[0];
      phi   = angles[1];
      psi   = angles[2];
      
      cosTheta = cos(theta);  cosPhi = cos(phi);  cosPsi = cos(psi);
      sinTheta = sin(theta);  sinPhi = sin(phi);  sinPsi = sin(psi);
      
      /*--- Compute the rotation matrix. Note that the implicit
       ordering is rotation about the x-axis, y-axis, then z-axis. ---*/
      rotMatrix[0][0] = cosPhi*cosPsi;
      rotMatrix[1][0] = cosPhi*sinPsi;
      rotMatrix[2][0] = -sinPhi;
      
      rotMatrix[0][1] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;
      rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;
      rotMatrix[2][1] = sinTheta*cosPhi;
      
      rotMatrix[0][2] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
      rotMatrix[1][2] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
      rotMatrix[2][2] = cosTheta*cosPhi;
      
      /*--- Loop through all vertices and find/set the periodic point. ---*/
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        
        /*--- Retrieve node information for this boundary point. ---*/
        iPoint  = vertex[iMarker][iVertex]->GetNode();
        Coord_i = node[iPoint]->GetCoord();
        
        /*--- Get the position vector from rot center to point. ---*/
        dx = Coord_i[0] - center[0];
        dy = Coord_i[1] - center[1];
        if (nDim == 3) {
          dz = Coord_i[2] - center[2];
        } else {
          dz = 0.0;
        }
        
        /*--- Compute transformed point coordinates. ---*/
        rotCoord[0] = rotMatrix[0][0]*dx
        + rotMatrix[0][1]*dy
        + rotMatrix[0][2]*dz + translation[0];
        
        rotCoord[1] = rotMatrix[1][0]*dx
        + rotMatrix[1][1]*dy
        + rotMatrix[1][2]*dz + translation[1];
        
        rotCoord[2] = rotMatrix[2][0]*dx
        + rotMatrix[2][1]*dy
        + rotMatrix[2][2]*dz + translation[2];
        
        /*--- Perform a search to find the closest donor point. ---*/
        mindist = 1e10;
        for (jVertex = 0; jVertex < nVertex[jMarker]; jVertex++) {
          
          /*--- Retrieve information for this jPoint. ---*/
          jPoint = vertex[jMarker][jVertex]->GetNode();
          Coord_j = node[jPoint]->GetCoord();
          
          /*--- Check the distance between the computed periodic
           location and this jPoint. ---*/
          dist = 0.0;
          for (iDim = 0; iDim < nDim; iDim++) {
            dist += (Coord_j[iDim]-rotCoord[iDim])*(Coord_j[iDim]-rotCoord[iDim]);
          }
          dist = sqrt(dist);
          
          /*---  Store vertex information if this is the closest
           point found thus far. ---*/
          if (dist < mindist) { mindist = dist; pPoint = jPoint; }
        }
        
        /*--- Set the periodic point for this iPoint. ---*/
        vertex[iMarker][iVertex]->SetDonorPoint(pPoint, MASTER_NODE);
        
        /*--- Print warning if the nearest point was not within
         the specified tolerance. Computation will continue. ---*/
        if (mindist > epsilon) {
          isBadMatch = true;
          cout.precision(10);
          cout << endl;
          cout << "   Bad match for point " << iPoint << ".\tNearest";
          cout << " donor distance: " << scientific << mindist << ".";
        }
      }
      
      /*--- Print final warning when finding bad matches. ---*/
      if (isBadMatch) {
        cout << endl;
        cout << "\n !!! Warning !!!" << endl;
        cout << "Bad matches found. Computation will continue, but be cautious.\n";
      }
      cout << endl;
      isBadMatch = false;
      
    }
  
  /*--- Create a vector to identify the points that belong to each periodic boundary condition ---*/
  bool *PeriodicBC = new bool [nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++) PeriodicBC[iPoint] = false;
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY)
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        PeriodicBC[iPoint] = true;
      }
  
  /*--- Determine the new points that must be added to each periodic boundary,
   note that only one of the boundaries require the extra data ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY) {
      iPeriodic = config->GetMarker_All_PerBound(iMarker);
      
      /*--- An integer identify the periodic boundary condition --*/
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        
        /*--- iPoint is the original point on the surface and jPoint is the
         equivalent point in the other periodic surface ---*/
        iPoint = vertex[iMarker][iVertex]->GetNode();
        jPoint = vertex[iMarker][iVertex]->GetDonorPoint();
        
        /*--- First the case in which it is necessary to create a mirror set of elements ---*/
        if (CreateMirror[iPeriodic]) {
          /*--- Now we must determine the neighbor points (including indirect ones) to the periodic points
           and store all the information (in this list we do not include the points
           that already belong to the periodic boundary), we also add the elements that
           share a point with the periodic boundary condition ---*/
          for (iIndex = 0; iIndex < node[jPoint]->GetnElem(); iIndex++) {
            iElem = node[jPoint]->GetElem(iIndex);
            PeriodicElem[iPeriodic].push_back(iElem);
            for (unsigned short iNode = 0; iNode <	elem[iElem]->GetnNodes(); iNode ++) {
              kPoint = elem[iElem]->GetNode(iNode);
              if (!PeriodicBC[kPoint]) PeriodicPoint[iPeriodic][0].push_back(kPoint);
            }
          }
        }
        /*--- Second the case where no new element is added, neither points ---*/
        else {
          PeriodicPoint[iPeriodic][0].push_back(jPoint);
          PeriodicPoint[iPeriodic][1].push_back(iPoint);
        }
      }
    }
  }
  
  /*--- Sort the points that must be sended and delete repeated points ---*/
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      sort( PeriodicPoint[iPeriodic][0].begin(), PeriodicPoint[iPeriodic][0].end());
      IterPoint[iPeriodic][0] = unique( PeriodicPoint[iPeriodic][0].begin(), PeriodicPoint[iPeriodic][0].end());
      PeriodicPoint[iPeriodic][0].resize( IterPoint[iPeriodic][0] - PeriodicPoint[iPeriodic][0].begin() );
    }
  }
  
  /*--- Create a list of the points that receive the values (only the new points) ---*/
  nPointPeriodic = nPoint;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      for (iPoint = 0; iPoint < PeriodicPoint[iPeriodic][0].size(); iPoint++) {
        PeriodicPoint[iPeriodic][1].push_back(nPointPeriodic);
        nPointPeriodic++;
      }
    }
  }
  
  /*--- Sort the elements that must be replicated in the periodic boundary
   and delete the repeated elements ---*/
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      sort( PeriodicElem[iPeriodic].begin(), PeriodicElem[iPeriodic].end());
      IterElem = unique( PeriodicElem[iPeriodic].begin(), PeriodicElem[iPeriodic].end());
      PeriodicElem[iPeriodic].resize( IterElem - PeriodicElem[iPeriodic].begin() );
    }
  }
  
  /*--- Check all SEND points to see if they also lie on another boundary. ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      
      /*--- iPoint is a node that lies on the current marker. ---*/
      iPoint = vertex[iMarker][iVertex]->GetNode();
      
      /*--- Search through SEND points to check for iPoint. ---*/
      for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
        if (CreateMirror[iPeriodic]) {
          
          /*--- jPoint is the SEND point. ---*/
          for (iElem = 0; iElem < PeriodicPoint[iPeriodic][0].size(); iElem++) {
            jPoint = PeriodicPoint[iPeriodic][0][iElem];
            
            /*--- If the two match, then jPoint lies on this boundary.
             However, we are concerned with the new points, so we
             will store kPoint instead. ---*/
            if (iPoint == jPoint) {
//              kPoint = PeriodicPoint[iPeriodic][1][iElem];
              
              /*--- We also want the type of boundary element that this point
               was within, so that we know what type of element to add
               built from the new points. ---*/
              bool isJPoint, isPeriodic;
              for (jElem = 0; jElem < nElem_Bound[iMarker]; jElem++) {
                isJPoint = false; isPeriodic = false;
                for (iNode = 0; iNode < bound[iMarker][jElem]->GetnNodes(); iNode++) {
                  if (bound[iMarker][jElem]->GetNode(iNode) == jPoint) isJPoint = true;
                  if (PeriodicBC[bound[iMarker][jElem]->GetNode(iNode)]) isPeriodic = true;
                }
                
                /*--- If both points were found, store this element. ---*/
                if (isJPoint && isPeriodic) {
                  OldBoundaryElems[iMarker].push_back(jElem);
                }
                
              }
              
            }
          }
        }
      }
    }
  }
  
  /*--- Sort the elements that must be added and remove duplicates. ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    sort( OldBoundaryElems[iMarker].begin(), OldBoundaryElems[iMarker].end());
    IterNewElem[iMarker] = unique( OldBoundaryElems[iMarker].begin(), OldBoundaryElems[iMarker].end());
    OldBoundaryElems[iMarker].resize( IterNewElem[iMarker] - OldBoundaryElems[iMarker].begin() );
  }
  
  /*--- Create the new boundary elements. Points making up these new
   elements must either be SEND/RECEIVE or periodic points. ---*/
  nNewElem_Bound = new unsigned long[nMarker];
  newBound = new CPrimalGrid**[nMarker];
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    nNewElem_Bound[iMarker] = OldBoundaryElems[iMarker].size();
    newBound[iMarker] = new CPrimalGrid*[nNewElem_Bound[iMarker]];
    
    /*--- Loop through all new elements to be added. ---*/
    for (iElem = 0; iElem < nNewElem_Bound[iMarker]; iElem++) {
      jElem = OldBoundaryElems[iMarker][iElem];
      
      /*--- Loop through all nodes of this element. ---*/
      for (iNode = 0; iNode < bound[iMarker][jElem]->GetnNodes(); iNode++) {
        pPoint = bound[iMarker][jElem]->GetNode(iNode);
        
        /*--- Check if this node is a send point. If so, the corresponding
         receive point will be used in making the new boundary element. ---*/
        for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
          for (kElem = 0; kElem < PeriodicPoint[iPeriodic][0].size(); kElem++) {
            if (pPoint == PeriodicPoint[iPeriodic][0][kElem]) newNodes[iNode] = PeriodicPoint[iPeriodic][1][kElem];
          }
        }
        
        /*--- Check if this node is a periodic point. If so, the corresponding
         periodic point will be used in making the new boundary element. ---*/
        if (PeriodicBC[pPoint]) {
          
          /*--- Find the corresponding periodic point. ---*/
          for (jMarker = 0; jMarker < config->GetnMarker_All(); jMarker++) {
            if (config->GetMarker_All_KindBC(jMarker) == PERIODIC_BOUNDARY) {
              for (iVertex = 0; iVertex < nVertex[jMarker]; iVertex++) {
                if (pPoint == vertex[jMarker][iVertex]->GetNode()) {kMarker = jMarker; jVertex = iVertex;}
              }
            }
          }
          newNodes[iNode] = vertex[kMarker][jVertex]->GetDonorPoint();
        }
      }
      
      /*--- Now instantiate the new element. ---*/
      VTK_Type = bound[iMarker][jElem]->GetVTK_Type();
      switch(VTK_Type) {
        case LINE:
          newBound[iMarker][iElem] = new CLine(newNodes[0], newNodes[1],2);
          break;
        case TRIANGLE:
          newBound[iMarker][iElem] = new CTriangle(newNodes[0], newNodes[1], newNodes[2],3);
          break;
        case QUADRILATERAL:
          newBound[iMarker][iElem] = new CQuadrilateral(newNodes[0], newNodes[1], newNodes[2], newNodes[3],3);
          break;
      }
      
    }
  }
  
  delete [] PeriodicBC;
  delete [] CreateMirror;
  
}

void CPhysicalGeometry::FindNormal_Neighbor(CConfig *config) {
  su2double cos_max, scalar_prod, norm_vect, norm_Normal, cos_alpha, diff_coord, *Normal;
  unsigned long Point_Normal, jPoint;
  unsigned short iNeigh, iMarker, iDim;
  unsigned long iPoint, iVertex;
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE &&
        config->GetMarker_All_KindBC(iMarker) != INTERFACE_BOUNDARY &&
        config->GetMarker_All_KindBC(iMarker) != NEARFIELD_BOUNDARY ) {
      
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        
        iPoint = vertex[iMarker][iVertex]->GetNode();
        Normal = vertex[iMarker][iVertex]->GetNormal();
        
        /*--- Compute closest normal neighbor, note that the normal are oriented inwards ---*/
        Point_Normal = 0; cos_max = -1.0;
        for (iNeigh = 0; iNeigh < node[iPoint]->GetnPoint(); iNeigh++) {
          jPoint = node[iPoint]->GetPoint(iNeigh);
          scalar_prod = 0.0; norm_vect = 0.0; norm_Normal = 0.0;
          for (iDim = 0; iDim < nDim; iDim++) {
            diff_coord = node[jPoint]->GetCoord(iDim)-node[iPoint]->GetCoord(iDim);
            scalar_prod += diff_coord*Normal[iDim];
            norm_vect += diff_coord*diff_coord;
            norm_Normal += Normal[iDim]*Normal[iDim];
          }
          norm_vect = sqrt(norm_vect);
          norm_Normal = sqrt(norm_Normal);
          cos_alpha = scalar_prod/(norm_vect*norm_Normal);
          
          /*--- Get maximum cosine ---*/
          if (cos_alpha >= cos_max) {
            Point_Normal = jPoint;
            cos_max = cos_alpha;
          }
        }
        vertex[iMarker][iVertex]->SetNormal_Neighbor(Point_Normal);
      }
    }
  }
}

void CPhysicalGeometry::SetGeometryPlanes(CConfig *config) {
  
  bool loop_on;
  unsigned short iMarker = 0;
  su2double auxXCoord, auxYCoord, auxZCoord,	*Face_Normal = NULL, auxArea, *Xcoord = NULL, *Ycoord = NULL, *Zcoord = NULL, *FaceArea = NULL;
  unsigned long jVertex, iVertex, ixCoord, iPoint, iVertex_Wall, nVertex_Wall = 0;
  
  /*--- Compute the total number of points on the near-field ---*/
  nVertex_Wall = 0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if ((config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)               ||
        (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)              ||
        (config->GetMarker_All_KindBC(iMarker) == EULER_WALL)                )
      nVertex_Wall += nVertex[iMarker];
  
  
  /*--- Create an array with all the coordinates, points, pressures, face area,
   equivalent area, and nearfield weight ---*/
  Xcoord = new su2double[nVertex_Wall];
  Ycoord = new su2double[nVertex_Wall];
  if (nDim == 3)	Zcoord = new su2double[nVertex_Wall];
  FaceArea = new su2double[nVertex_Wall];
  
  /*--- Copy the boundary information to an array ---*/
  iVertex_Wall = 0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if ((config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)               ||
        (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)              ||
        (config->GetMarker_All_KindBC(iMarker) == EULER_WALL)                )
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        Xcoord[iVertex_Wall] = node[iPoint]->GetCoord(0);
        Ycoord[iVertex_Wall] = node[iPoint]->GetCoord(1);
        if (nDim==3) Zcoord[iVertex_Wall] = node[iPoint]->GetCoord(2);
        Face_Normal = vertex[iMarker][iVertex]->GetNormal();
        FaceArea[iVertex_Wall] = fabs(Face_Normal[nDim-1]);
        iVertex_Wall ++;
      }
  
  
  //vector<su2double> XCoordList;
  vector<su2double>::iterator IterXCoordList;
  
  for (iVertex = 0; iVertex < nVertex_Wall; iVertex++)
    XCoordList.push_back(Xcoord[iVertex]);
  
  sort( XCoordList.begin(), XCoordList.end());
  IterXCoordList = unique( XCoordList.begin(), XCoordList.end());
  XCoordList.resize( IterXCoordList - XCoordList.begin() );
  
  /*--- Create vectors and distribute the values among the different PhiAngle queues ---*/
  Xcoord_plane.resize(XCoordList.size());
  Ycoord_plane.resize(XCoordList.size());
  if (nDim==3) Zcoord_plane.resize(XCoordList.size());
  FaceArea_plane.resize(XCoordList.size());
  Plane_points.resize(XCoordList.size());
  
  
  su2double dist_ratio;
  unsigned long iCoord;
  
  /*--- Distribute the values among the different PhiAngles ---*/
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    if (node[iPoint]->GetDomain()) {
      loop_on = true;
      for (ixCoord = 0; ixCoord < XCoordList.size()-1 && loop_on; ixCoord++) {
        dist_ratio = (node[iPoint]->GetCoord(0) - XCoordList[ixCoord])/(XCoordList[ixCoord+1]- XCoordList[ixCoord]);
        if (dist_ratio >= 0 && dist_ratio <= 1.0) {
          if (dist_ratio <= 0.5) iCoord = ixCoord;
          else iCoord = ixCoord+1;
          Xcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(0) );
          Ycoord_plane[iCoord].push_back(node[iPoint]->GetCoord(1) );
          if (nDim==3) Zcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(2) );
          FaceArea_plane[iCoord].push_back(node[iPoint]->GetVolume());   ///// CHECK AREA CALCULATION
          Plane_points[iCoord].push_back(iPoint );
          loop_on = false;
        }
      }
    }
  }
  
  unsigned long auxPoint;
  /*--- Order the arrays in ascending values of y ---*/
  for (ixCoord = 0; ixCoord < XCoordList.size(); ixCoord++)
    for (iVertex = 0; iVertex < Xcoord_plane[ixCoord].size(); iVertex++)
      for (jVertex = 0; jVertex < Xcoord_plane[ixCoord].size() - 1 - iVertex; jVertex++)
        if (Ycoord_plane[ixCoord][jVertex] > Ycoord_plane[ixCoord][jVertex+1]) {
          auxXCoord = Xcoord_plane[ixCoord][jVertex]; Xcoord_plane[ixCoord][jVertex] = Xcoord_plane[ixCoord][jVertex+1]; Xcoord_plane[ixCoord][jVertex+1] = auxXCoord;
          auxYCoord = Ycoord_plane[ixCoord][jVertex]; Ycoord_plane[ixCoord][jVertex] = Ycoord_plane[ixCoord][jVertex+1]; Ycoord_plane[ixCoord][jVertex+1] = auxYCoord;
          auxPoint = Plane_points[ixCoord][jVertex]; Plane_points[ixCoord][jVertex] = Plane_points[ixCoord][jVertex+1]; Plane_points[ixCoord][jVertex+1] = auxPoint;
          if (nDim==3) {
            auxZCoord = Zcoord_plane[ixCoord][jVertex]; Zcoord_plane[ixCoord][jVertex] = Zcoord_plane[ixCoord][jVertex+1]; Zcoord_plane[ixCoord][jVertex+1] = auxZCoord;
          }
          auxArea = FaceArea_plane[ixCoord][jVertex]; FaceArea_plane[ixCoord][jVertex] = FaceArea_plane[ixCoord][jVertex+1]; FaceArea_plane[ixCoord][jVertex+1] = auxArea;
        }
  
  /*--- Delete structures ---*/
  delete[] Xcoord; delete[] Ycoord;
  if (Zcoord != NULL) delete[] Zcoord;
  delete[] FaceArea;
}

void CPhysicalGeometry::SetBoundSensitivity(CConfig *config) {
  unsigned short iMarker, icommas;
  unsigned long iVertex, iPoint, (*Point2Vertex)[2], nPointLocal = 0, nPointGlobal = 0;
  su2double Sensitivity;
  bool *PointInDomain;
  
#ifdef HAVE_MPI
  int rank = MASTER_NODE;
  int size = SINGLE_NODE;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif
  
  nPointLocal = nPoint;
#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&nPointLocal, &nPointGlobal, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  nPointGlobal = nPointLocal;
#endif
  
  Point2Vertex = new unsigned long[nPointGlobal][2];
  PointInDomain = new bool[nPointGlobal];
  
  for (iPoint = 0; iPoint < nPointGlobal; iPoint ++)
    PointInDomain[iPoint] = false;
  
  for (iMarker = 0; iMarker < nMarker; iMarker++)
    if (config->GetMarker_All_DV(iMarker) == YES)
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        
        /*--- The sensitivity file uses the global numbering ---*/
        iPoint = node[vertex[iMarker][iVertex]->GetNode()]->GetGlobalIndex();

        if (vertex[iMarker][iVertex]->GetNode() < GetnPointDomain()) {
          Point2Vertex[iPoint][0] = iMarker;
          Point2Vertex[iPoint][1] = iVertex;
          PointInDomain[iPoint] = true;
          vertex[iMarker][iVertex]->SetAuxVar(0.0);
        }
      }
  
  /*--- Time-average any unsteady surface sensitivities ---*/
  
  unsigned long iExtIter, nExtIter;
  su2double delta_T, total_T;
  if (config->GetUnsteady_Simulation() && config->GetWrt_Unsteady()) {
    nExtIter = config->GetUnst_AdjointIter();
    delta_T  = config->GetDelta_UnstTime();
    total_T  = (su2double)nExtIter*delta_T;
  } else if (config->GetUnsteady_Simulation() == HARMONIC_BALANCE) {
    
    /*--- Compute period of oscillation & compute time interval using nTimeInstances ---*/
    
    su2double period = config->GetHarmonicBalance_Period();
    nExtIter  = config->GetnTimeInstances();
    delta_T   = period/(su2double)nExtIter;
    total_T   = period;
    
  } else {
    nExtIter = 1;
    delta_T  = 1.0;
    total_T  = 1.0;
  }
  
  for (iExtIter = 0; iExtIter < nExtIter; iExtIter++) {
    
    /*--- Prepare to read surface sensitivity files (CSV) ---*/
    
    string text_line;
    ifstream Surface_file;
    char buffer[50];
    char cstr[MAX_STRING_SIZE];
    string surfadj_filename = config->GetSurfAdjCoeff_FileName();
    strcpy (cstr, surfadj_filename.c_str());
    
    /*--- Write file name with extension if unsteady or steady ---*/
    if (config->GetUnsteady_Simulation() == HARMONIC_BALANCE)
    	SPRINTF (buffer, "_%d.csv", SU2_TYPE::Int(iExtIter));

    if ((config->GetUnsteady_Simulation() && config->GetWrt_Unsteady()) ||
        (config->GetUnsteady_Simulation() == HARMONIC_BALANCE)) {
      if ((SU2_TYPE::Int(iExtIter) >= 0)    && (SU2_TYPE::Int(iExtIter) < 10))    SPRINTF (buffer, "_0000%d.csv", SU2_TYPE::Int(iExtIter));
      if ((SU2_TYPE::Int(iExtIter) >= 10)   && (SU2_TYPE::Int(iExtIter) < 100))   SPRINTF (buffer, "_000%d.csv",  SU2_TYPE::Int(iExtIter));
      if ((SU2_TYPE::Int(iExtIter) >= 100)  && (SU2_TYPE::Int(iExtIter) < 1000))  SPRINTF (buffer, "_00%d.csv",   SU2_TYPE::Int(iExtIter));
      if ((SU2_TYPE::Int(iExtIter) >= 1000) && (SU2_TYPE::Int(iExtIter) < 10000)) SPRINTF (buffer, "_0%d.csv",    SU2_TYPE::Int(iExtIter));
      if (SU2_TYPE::Int(iExtIter) >= 10000) SPRINTF (buffer, "_%d.csv", SU2_TYPE::Int(iExtIter));
    }
    else
      SPRINTF (buffer, ".csv");
    
    strcat (cstr, buffer);
    
    /*--- Read the sensitivity file ---*/
    
    string::size_type position;
    
    Surface_file.open(cstr, ios::in);
    
    /*--- Read extra inofmration ---*/
    
    getline(Surface_file, text_line);
    text_line.erase (0,9);
    su2double AoASens = atof(text_line.c_str());
    config->SetAoA_Sens(AoASens);
    
    /*--- File header ---*/
    
    getline(Surface_file, text_line);
    
    while (getline(Surface_file, text_line)) {
      for (icommas = 0; icommas < 50; icommas++) {
        position = text_line.find( ",", 0 );
        if (position!=string::npos) text_line.erase (position,1);
      }
      stringstream  point_line(text_line);
      point_line >> iPoint >> Sensitivity;
      
      if (PointInDomain[iPoint]) {
        
        /*--- Find the vertex for the Point and Marker ---*/
        
        iMarker = Point2Vertex[iPoint][0];
        iVertex = Point2Vertex[iPoint][1];
        
        /*--- Increment the auxiliary variable with the contribution of
         this unsteady timestep. For steady problems, this reduces to
         a single sensitivity value multiplied by 1.0. ---*/
        
        vertex[iMarker][iVertex]->AddAuxVar(Sensitivity*(delta_T/total_T));
      }
      
    }
    Surface_file.close();
  }
  
  delete[] Point2Vertex;
  delete[] PointInDomain;
  
}

void CPhysicalGeometry::SetSensitivity(CConfig *config) {
  
  ifstream restart_file;
  string filename = config->GetSolution_AdjFileName();
  bool compressible = (config->GetKind_Regime() == COMPRESSIBLE);
  bool incompressible = (config->GetKind_Regime() == INCOMPRESSIBLE);
  bool sst = config->GetKind_Turb_Model() == SST;
  bool sa = (config->GetKind_Turb_Model() == SA) || (config->GetKind_Turb_Model() == SA_NEG);
  bool grid_movement = config->GetGrid_Movement();
  bool frozen_visc = config->GetFrozen_Visc_Disc();
  su2double Sens, dull_val, AoASens;
  unsigned short nExtIter, iDim;
  unsigned long iPoint, index;
  string::size_type position;
  int counter = 0;
  
  Sensitivity = new su2double[nPoint*nDim];

  if (config->GetUnsteady_Simulation()) {
    nExtIter = config->GetnExtIter();
  }else {
    nExtIter = 1;
  }
    int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
 
  if (rank == MASTER_NODE)
    cout << "Reading in sensitivity at iteration " << nExtIter-1 << "."<< endl;
  
  unsigned short skipVar = nDim, skipMult = 1;

  if (incompressible)      { skipVar += skipMult*(nDim+1); }
  if (compressible)        { skipVar += skipMult*(nDim+2); }
  if (sst && !frozen_visc) { skipVar += skipMult*2;}
  if (sa && !frozen_visc)  { skipVar += skipMult*1;}
  if (grid_movement)       { skipVar += nDim;}

  /*--- Read all lines in the restart file ---*/
  long iPoint_Local; unsigned long iPoint_Global = 0; string text_line;
  
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++) {
      Sensitivity[iPoint*nDim+iDim] = 0.0;
    }
  }

  iPoint_Global = 0;

  filename = config->GetSolution_AdjFileName();

  filename = config->GetObjFunc_Extension(filename);

  if (config->GetUnsteady_Simulation()) {
    filename = config->GetUnsteady_FileName(filename, nExtIter-1);
  }

	if (config->GetnZone() > 1){
		filename = config->GetMultizone_FileName(filename, config->GetiZone());
	}

  if (config->GetRead_Binary_Restart()) {

    char str_buf[CGNS_STRING_SIZE], fname[100];
    unsigned short iVar;
    strcpy(fname, filename.c_str());
    int nRestart_Vars = 5, nFields;
    int *Restart_Vars = new int[5];
    passivedouble *Restart_Data = NULL;
    int Restart_Iter = 0;
    passivedouble Restart_Meta_Passive[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
    su2double Restart_Meta[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};

#ifndef HAVE_MPI

    /*--- Serial binary input. ---*/

    FILE *fhw;
    fhw = fopen(fname,"rb");

    /*--- Error check for opening the file. ---*/

    if (!fhw) {
      cout << endl << "Error: unable to open SU2 restart file " << fname << "." << endl;
      exit(EXIT_FAILURE);
    }

    /*--- First, read the number of variables and points. ---*/

    fread(Restart_Vars, sizeof(int), nRestart_Vars, fhw);

    /*--- Check that this is an SU2 binary file. SU2 binary files
     have the hex representation of "SU2" as the first int in the file. ---*/

    if (Restart_Vars[0] != 535532) {
      cout << endl << endl << "Error: file " << fname << " is not a binary SU2 restart file." << endl;
      cout << " SU2 reads/writes binary restart files by default." << endl;
      cout << " Note that backward compatibility for ASCII restart files is" << endl;
      cout << " possible with the WRT_BINARY_RESTART / READ_BINARY_RESTART options." << endl << endl;
      exit(EXIT_FAILURE);
    }

    /*--- Store the number of fields for simplicity. ---*/

    nFields = Restart_Vars[1];

    /*--- Read the variable names from the file. Note that we are adopting a
     fixed length of 33 for the string length to match with CGNS. This is
     needed for when we read the strings later. We pad the beginning of the
     variable string vector with the Point_ID tag that wasn't written. ---*/

    config->fields.push_back("Point_ID");
    for (iVar = 0; iVar < nFields; iVar++) {
      fread(str_buf, sizeof(char), CGNS_STRING_SIZE, fhw);
      config->fields.push_back(str_buf);
    }

    /*--- For now, create a temp 1D buffer to read the data from file. ---*/

    Restart_Data = new passivedouble[nFields*GetnPointDomain()];

    /*--- Read in the data for the restart at all local points. ---*/

    fread(Restart_Data, sizeof(passivedouble), nFields*GetnPointDomain(), fhw);

    /*--- Compute (negative) displacements and grab the metadata. ---*/

    fseek(fhw,-(sizeof(int) + 8*sizeof(passivedouble)), SEEK_END);

    /*--- Read the external iteration. ---*/

    fread(&Restart_Iter, 1, sizeof(int), fhw);

    /*--- Read the metadata. ---*/

    fread(Restart_Meta_Passive, 8, sizeof(passivedouble), fhw);
    
    /*--- Close the file. ---*/

    fclose(fhw);

#else

    /*--- Parallel binary input using MPI I/O. ---*/

    MPI_File fhw;
    MPI_Status status;
    MPI_Datatype etype, filetype;
    MPI_Offset disp;
    unsigned long iPoint_Global, iChar;
    string field_buf;

    int rank = MASTER_NODE, ierr;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*--- All ranks open the file using MPI. ---*/

    ierr = MPI_File_open(MPI_COMM_WORLD, fname, MPI_MODE_RDONLY, MPI_INFO_NULL, &fhw);

    /*--- Error check opening the file. ---*/

    if (ierr) {
      if (rank == MASTER_NODE)
        cout << endl << "Error: unable to open SU2 restart file " << fname << "." << endl;
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
    }

    /*--- First, read the number of variables and points (i.e., cols and rows),
     which we will need in order to read the file later. Also, read the
     variable string names here. Only the master rank reads the header. ---*/
    
    if (rank == MASTER_NODE)
      MPI_File_read(fhw, Restart_Vars, nRestart_Vars, MPI_INT, MPI_STATUS_IGNORE);

    /*--- Broadcast the number of variables to all procs and store clearly. ---*/

    SU2_MPI::Bcast(Restart_Vars, nRestart_Vars, MPI_INT, MASTER_NODE, MPI_COMM_WORLD);

    /*--- Check that this is an SU2 binary file. SU2 binary files
     have the hex representation of "SU2" as the first int in the file. ---*/

    if (Restart_Vars[0] != 535532) {
      if (rank == MASTER_NODE) {
        cout << endl << endl << "Error: file " << fname << " is not a binary SU2 restart file." << endl;
        cout << " SU2 reads/writes binary restart files by default." << endl;
        cout << " Note that backward compatibility for ASCII restart files is" << endl;
        cout << " possible with the WRT_BINARY_RESTART / READ_BINARY_RESTART options." << endl << endl;
      }
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
    }

    /*--- Store the number of fields for simplicity. ---*/

    nFields = Restart_Vars[1];

    /*--- Read the variable names from the file. Note that we are adopting a
     fixed length of 33 for the string length to match with CGNS. This is
     needed for when we read the strings later. ---*/

    char *mpi_str_buf = new char[nFields*CGNS_STRING_SIZE];
    if (rank == MASTER_NODE) {
      disp = nRestart_Vars*sizeof(int);
      MPI_File_read_at(fhw, disp, mpi_str_buf, nFields*CGNS_STRING_SIZE,
                       MPI_CHAR, MPI_STATUS_IGNORE);
    }

    /*--- Broadcast the string names of the variables. ---*/

    SU2_MPI::Bcast(mpi_str_buf, nFields*CGNS_STRING_SIZE, MPI_CHAR,
                   MASTER_NODE, MPI_COMM_WORLD);

    /*--- Now parse the string names and load into the config class in case
     we need them for writing visualization files (SU2_SOL). ---*/

    config->fields.push_back("Point_ID");
    for (iVar = 0; iVar < nFields; iVar++) {
      index = iVar*CGNS_STRING_SIZE;
      field_buf.append("\"");
      for (iChar = 0; iChar < CGNS_STRING_SIZE; iChar++) {
        str_buf[iChar] = mpi_str_buf[index + iChar];
      }
      field_buf.append(str_buf);
      field_buf.append("\"");
      config->fields.push_back(field_buf.c_str());
      field_buf.clear();
    }

    /*--- Free string buffer memory. ---*/

    delete [] mpi_str_buf;

    /*--- We're writing only su2doubles in the data portion of the file. ---*/

    etype = MPI_DOUBLE;

    /*--- We need to ignore the 4 ints describing the nVar_Restart and nPoints,
     along with the string names of the variables. ---*/

    disp = nRestart_Vars*sizeof(int) + CGNS_STRING_SIZE*nFields*sizeof(char);

    /*--- Define a derived datatype for this rank's set of non-contiguous data
     that will be placed in the restart. Here, we are collecting each one of the
     points which are distributed throughout the file in blocks of nVar_Restart data. ---*/

    int *blocklen = new int[GetnPointDomain()];
    int *displace = new int[GetnPointDomain()];

    counter = 0;
    for (iPoint_Global = 0; iPoint_Global < GetGlobal_nPointDomain(); iPoint_Global++ ) {
      if (GetGlobal_to_Local_Point(iPoint_Global) > -1) {
        blocklen[counter] = nFields;
        displace[counter] = iPoint_Global*nFields;
        counter++;
      }
    }
    MPI_Type_indexed(GetnPointDomain(), blocklen, displace, MPI_DOUBLE, &filetype);
    MPI_Type_commit(&filetype);

    /*--- Set the view for the MPI file write, i.e., describe the location in
     the file that this rank "sees" for writing its piece of the restart file. ---*/

    MPI_File_set_view(fhw, disp, etype, filetype, "native", MPI_INFO_NULL);

    /*--- For now, create a temp 1D buffer to read the data from file. ---*/

    Restart_Data = new passivedouble[nFields*GetnPointDomain()];

    /*--- Collective call for all ranks to read from their view simultaneously. ---*/
    
    MPI_File_read_all(fhw, Restart_Data, nFields*GetnPointDomain(), MPI_DOUBLE, &status);

    /*--- Free the derived datatype. ---*/

    MPI_Type_free(&filetype);

    /*--- Reset the file view before writing the metadata. ---*/

    MPI_File_set_view(fhw, 0, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);

    /*--- Access the metadata. ---*/

    if (rank == MASTER_NODE) {

      /*--- External iteration. ---*/
      disp = (nRestart_Vars*sizeof(int) + nFields*CGNS_STRING_SIZE*sizeof(char) +
              nFields*Restart_Vars[2]*sizeof(passivedouble));
      MPI_File_read_at(fhw, disp, &Restart_Iter, 1, MPI_INT, MPI_STATUS_IGNORE);

      /*--- Additional doubles for AoA, AoS, etc. ---*/

      disp = (nRestart_Vars*sizeof(int) + nFields*CGNS_STRING_SIZE*sizeof(char) +
              nFields*Restart_Vars[2]*sizeof(passivedouble) + 1*sizeof(int));
      MPI_File_read_at(fhw, disp, Restart_Meta_Passive, 8, MPI_DOUBLE, MPI_STATUS_IGNORE);

    }

    /*--- Communicate metadata. ---*/

    SU2_MPI::Bcast(&Restart_Iter, 1, MPI_INT, MASTER_NODE, MPI_COMM_WORLD);

    /*--- Copy to a su2double structure (because of the SU2_MPI::Bcast
              doesn't work with passive data)---*/

    for (unsigned short iVar = 0; iVar < 8; iVar++)
      Restart_Meta[iVar] = Restart_Meta_Passive[iVar];

    SU2_MPI::Bcast(Restart_Meta, 8, MPI_DOUBLE, MASTER_NODE, MPI_COMM_WORLD);

    /*--- All ranks close the file after writing. ---*/
    
    MPI_File_close(&fhw);
    
    delete [] blocklen;
    delete [] displace;
    
#endif

    /*--- Load the data from the binary restart. ---*/

    counter = 0;
    for (iPoint_Global = 0; iPoint_Global < GetGlobal_nPointDomain(); iPoint_Global++ ) {

      /*--- Retrieve local index. If this node from the restart file lives
       on the current processor, we will load and instantiate the vars. ---*/

      iPoint_Local = GetGlobal_to_Local_Point(iPoint_Global);

      if (iPoint_Local > -1) {

        /*--- We need to store this point's data, so jump to the correct
         offset in the buffer of data from the restart file and load it. ---*/

        index = counter*nFields + skipVar;
        for (iDim = 0; iDim < nDim; iDim++) Sensitivity[iPoint_Local*nDim+iDim] = Restart_Data[index+iDim];

        /*--- Increment the overall counter for how many points have been loaded. ---*/
        counter++;
      }
    }

    /*--- Lastly, load the AoA sensitivity from the binary metadata. ---*/

    config->SetAoA_Sens(Restart_Meta[4]);

  } else {

    /*--- First, check that this is not a binary restart file. ---*/

    char fname[100];
    strcpy(fname, filename.c_str());
    int magic_number;

#ifndef HAVE_MPI

    /*--- Serial binary input. ---*/

    FILE *fhw;
    fhw = fopen(fname,"rb");

    /*--- Error check for opening the file. ---*/

    if (!fhw) {
      cout << endl << "Error: unable to open SU2 restart file " << fname << "." << endl;
      exit(EXIT_FAILURE);
    }

    /*--- Attempt to read the first int, which should be our magic number. ---*/

    fread(&magic_number, sizeof(int), 1, fhw);

    /*--- Check that this is an SU2 binary file. SU2 binary files
     have the hex representation of "SU2" as the first int in the file. ---*/

    if (magic_number == 535532) {
      cout << endl << endl << "Error: file " << fname << " is a binary SU2 restart file, expected ASCII." << endl;
      cout << " SU2 reads/writes binary restart files by default." << endl;
      cout << " Note that backward compatibility for ASCII restart files is" << endl;
      cout << " possible with the WRT_BINARY_RESTART / READ_BINARY_RESTART options." << endl << endl;
      exit(EXIT_FAILURE);
    }

    fclose(fhw);

#else

    /*--- Parallel binary input using MPI I/O. ---*/

    MPI_File fhw;
    int ierr;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*--- All ranks open the file using MPI. ---*/

    ierr = MPI_File_open(MPI_COMM_WORLD, fname, MPI_MODE_RDONLY, MPI_INFO_NULL, &fhw);

    /*--- Error check opening the file. ---*/

    if (ierr) {
      if (rank == MASTER_NODE)
        cout << endl << "Error: unable to open SU2 restart file " << fname << "." << endl;
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
    }

    /*--- Have the master attempt to read the magic number. ---*/

    if (rank == MASTER_NODE)
      MPI_File_read(fhw, &magic_number, 1, MPI_INT, MPI_STATUS_IGNORE);

    /*--- Broadcast the number of variables to all procs and store clearly. ---*/

    SU2_MPI::Bcast(&magic_number, 1, MPI_INT, MASTER_NODE, MPI_COMM_WORLD);

    /*--- Check that this is an SU2 binary file. SU2 binary files
     have the hex representation of "SU2" as the first int in the file. ---*/

    if (magic_number == 535532) {
      if (rank == MASTER_NODE) {
        cout << endl << endl << "Error: file " << fname << " is a binary SU2 restart file, expected ASCII." << endl;
        cout << " SU2 reads/writes binary restart files by default." << endl;
        cout << " Note that backward compatibility for ASCII restart files is" << endl;
        cout << " possible with the WRT_BINARY_RESTART / READ_BINARY_RESTART options." << endl << endl;
      }
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
    }
    
    MPI_File_close(&fhw);
    
#endif

  restart_file.open(filename.data(), ios::in);
  if (restart_file.fail()) {
    if (rank == MASTER_NODE) cout << "There is no adjoint restart file!! " << filename.data() << "."<< endl;
#ifndef HAVE_MPI
      exit(EXIT_FAILURE);
#else
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Abort(MPI_COMM_WORLD,1);
      MPI_Finalize();
#endif
  }
  
  /*--- The first line is the header ---*/

  getline (restart_file, text_line);
  
  for (iPoint_Global = 0; iPoint_Global < GetGlobal_nPointDomain(); iPoint_Global++ ) {

    getline (restart_file, text_line);

  	istringstream point_line(text_line);

    /*--- Retrieve local index. If this node from the restart file lives
     on the current processor, we will load and instantiate the vars. ---*/

    iPoint_Local = GetGlobal_to_Local_Point(iPoint_Global);

    if (iPoint_Local > -1) {

      point_line >> index;
      for (iDim = 0; iDim < skipVar; iDim++) { point_line >> dull_val;}
      for (iDim = 0; iDim < nDim; iDim++) {
        point_line >> Sens;
        Sensitivity[iPoint_Local*nDim+iDim] = Sens;
      }
    }

  }
  
  /*--- Read AoA sensitivity ---*/
  
  while (getline (restart_file, text_line)) {
    position = text_line.find ("SENS_AOA=",0);
    if (position != string::npos) {
      text_line.erase (0,9); AoASens = atof(text_line.c_str());
      config->SetAoA_Sens(AoASens);
    }
  }
  
  restart_file.close();

  }
  
}

void CPhysicalGeometry::Check_Periodicity(CConfig *config) {
  
  bool isPeriodic = false;
  unsigned long iVertex;
  unsigned short iMarker, RotationKind, nPeriodicR = 0, nPeriodicS = 0;
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  /*--- Check for the presence of any periodic BCs ---*/
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        RotationKind = vertex[iMarker][iVertex]->GetRotation_Type();
        if (RotationKind > 0) nPeriodicS++;
      }
    }
  }
#ifndef HAVE_MPI
  nPeriodicR = nPeriodicS;
#else
  SU2_MPI::Allreduce(&nPeriodicS, &nPeriodicR, 1, MPI_UNSIGNED_SHORT, MPI_SUM, MPI_COMM_WORLD);
#endif
  if (nPeriodicR != 0) isPeriodic = true;
  
  if (isPeriodic && (config->GetnMGLevels() > 0)) {
    if (rank == MASTER_NODE)
      cout << "WARNING: Periodicity has been detected. Disabling multigrid. "<< endl;
    config->SetMGLevels(0);
  }
  
}

su2double CPhysicalGeometry::Compute_MaxThickness(su2double *Plane_P0, su2double *Plane_Normal, CConfig *config, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, jVertex, n, Trailing_Point, Leading_Point;
  su2double Normal[3], Tangent[3], BiNormal[3], auxXCoord, auxYCoord, auxZCoord, zp1, zpn, MaxThickness_Value = 0, Thickness, Length, Xcoord_Trailing, Ycoord_Trailing, Zcoord_Trailing, ValCos, ValSin, XValue, ZValue, MaxDistance, Distance, AoA;
  vector<su2double> Xcoord, Ycoord, Zcoord, Z2coord, Xcoord_Normal, Ycoord_Normal, Zcoord_Normal, Xcoord_Airfoil_, Ycoord_Airfoil_, Zcoord_Airfoil_;
  
  /*--- Find the leading and trailing edges and compute the angle of attack ---*/
  
  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }
  
  AoA = atan((Zcoord_Airfoil[Leading_Point] - Zcoord_Airfoil[Trailing_Point]) / (Xcoord_Airfoil[Trailing_Point] - Xcoord_Airfoil[Leading_Point]))*180/PI_NUMBER;
  
  /*--- Translate to the origin ---*/
  
  Xcoord_Trailing = Xcoord_Airfoil[0];
  Ycoord_Trailing = Ycoord_Airfoil[0];
  Zcoord_Trailing = Zcoord_Airfoil[0];
  
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Xcoord_Airfoil_.push_back(Xcoord_Airfoil[iVertex] - Xcoord_Trailing);
    Ycoord_Airfoil_.push_back(Ycoord_Airfoil[iVertex] - Ycoord_Trailing);
    Zcoord_Airfoil_.push_back(Zcoord_Airfoil[iVertex] - Zcoord_Trailing);
  }
  
  /*--- Rotate the airfoil ---*/
  
  ValCos = cos(AoA*PI_NUMBER/180.0);
  ValSin = sin(AoA*PI_NUMBER/180.0);
  
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    XValue = Xcoord_Airfoil_[iVertex];
    ZValue = Zcoord_Airfoil_[iVertex];
    Xcoord_Airfoil_[iVertex] = XValue*ValCos - ZValue*ValSin;
    Zcoord_Airfoil_[iVertex] = ZValue*ValCos + XValue*ValSin;
  }
  
  /*--- Identify upper and lower side, and store the value of the normal --*/
  
  for (iVertex = 1; iVertex < Xcoord_Airfoil_.size(); iVertex++) {
    Tangent[0] = Xcoord_Airfoil_[iVertex] - Xcoord_Airfoil_[iVertex-1];
    Tangent[1] = Ycoord_Airfoil_[iVertex] - Ycoord_Airfoil_[iVertex-1];
    Tangent[2] = Zcoord_Airfoil_[iVertex] - Zcoord_Airfoil_[iVertex-1];
    Length = sqrt(pow(Tangent[0], 2.0) + pow(Tangent[1], 2.0) + pow(Tangent[2], 2.0));

    Tangent[0] /= Length; Tangent[1] /= Length; Tangent[2] /= Length;
    
    BiNormal[0] = Plane_Normal[0];
    BiNormal[1] = Plane_Normal[1];
    BiNormal[2] = Plane_Normal[2];
    Length = sqrt(pow(BiNormal[0], 2.0) + pow(BiNormal[1], 2.0) + pow(BiNormal[2], 2.0));
    BiNormal[0] /= Length; BiNormal[1] /= Length; BiNormal[2] /= Length;
    
    Normal[0] = Tangent[1]*BiNormal[2] - Tangent[2]*BiNormal[1];
    Normal[1] = Tangent[2]*BiNormal[0] - Tangent[0]*BiNormal[2];
    Normal[2] = Tangent[0]*BiNormal[1] - Tangent[1]*BiNormal[0];
    
    Xcoord_Normal.push_back(Normal[0]); Ycoord_Normal.push_back(Normal[1]); Zcoord_Normal.push_back(Normal[2]);
    
    unsigned short index = 2;
    
    /*--- Removing the trailing edge from list of points that we are going to use in the interpolation,
			to be sure that a blunt trailing edge do not affect the interpolation ---*/

    if ((Normal[index] >= 0.0) && (fabs(Xcoord_Airfoil_[iVertex]) > MaxDistance*0.01)) {
      Xcoord.push_back(Xcoord_Airfoil_[iVertex]);
      Ycoord.push_back(Ycoord_Airfoil_[iVertex]);
      Zcoord.push_back(Zcoord_Airfoil_[iVertex]);
    }
    
  }
  
  /*--- Order the arrays using the X component ---*/
  
  for (iVertex = 0; iVertex < Xcoord.size(); iVertex++) {
    for (jVertex = 0; jVertex < Xcoord.size() - 1 - iVertex; jVertex++) {
      if (Xcoord[jVertex] > Xcoord[jVertex+1]) {
        auxXCoord = Xcoord[jVertex]; Xcoord[jVertex] = Xcoord[jVertex+1]; Xcoord[jVertex+1] = auxXCoord;
        auxYCoord = Ycoord[jVertex]; Ycoord[jVertex] = Ycoord[jVertex+1]; Ycoord[jVertex+1] = auxYCoord;
        auxZCoord = Zcoord[jVertex]; Zcoord[jVertex] = Zcoord[jVertex+1]; Zcoord[jVertex+1] = auxZCoord;
      }
    }
  }
  
  n = Xcoord.size();
  if (n > 1) {
    zp1 = (Zcoord[1]-Zcoord[0])/(Xcoord[1]-Xcoord[0]);
    zpn = (Zcoord[n-1]-Zcoord[n-2])/(Xcoord[n-1]-Xcoord[n-2]);
    Z2coord.resize(n+1);
    SetSpline(Xcoord, Zcoord, n, zp1, zpn, Z2coord);
    
    /*--- Compute the thickness (we add a fabs because we can not guarantee the
     right sorting of the points and the upper and/or lower part of the airfoil is not well defined) ---*/
    
    MaxThickness_Value = 0.0;
    for (iVertex = 0; iVertex < Xcoord_Airfoil_.size(); iVertex++) {
      if (Zcoord_Normal[iVertex] < 0.0) {
        Thickness = fabs(Zcoord_Airfoil_[iVertex] - GetSpline(Xcoord, Zcoord, Z2coord, n, Xcoord_Airfoil_[iVertex]));
        if (Thickness > MaxThickness_Value) { MaxThickness_Value = Thickness; }
      }
    }
  }
  else { MaxThickness_Value = 0.0; }

  return MaxThickness_Value;
  
}

su2double CPhysicalGeometry::Compute_Dihedral(su2double *LeadingEdge_im1, su2double *TrailingEdge_im1,
                                              su2double *LeadingEdge_i, su2double *TrailingEdge_i) {

  // su2double Dihedral_Leading = atan((LeadingEdge_i[2] - LeadingEdge_im1[2]) / (LeadingEdge_i[1] - LeadingEdge_im1[1]))*180/PI_NUMBER;
  su2double Dihedral_Trailing = atan((TrailingEdge_i[2] - TrailingEdge_im1[2]) / (TrailingEdge_i[1] - TrailingEdge_im1[1]))*180/PI_NUMBER;

  // su2double Dihedral = 0.5*(Dihedral_Leading + Dihedral_Trailing);

  return Dihedral_Trailing;

}

su2double CPhysicalGeometry::Compute_Curvature(su2double *LeadingEdge_im1, su2double *TrailingEdge_im1,
                                               su2double *LeadingEdge_i, su2double *TrailingEdge_i,
                                               su2double *LeadingEdge_ip1, su2double *TrailingEdge_ip1) {

  su2double A[2], B[2], C[2], BC[2], AB[2], AC[2], BC_MOD, AB_MOD,  AC_MOD, AB_CROSS_AC;

  // A[0] = LeadingEdge_im1[1];     A[1] = LeadingEdge_im1[2];
  // B[0] = LeadingEdge_i[1];           B[1] = LeadingEdge_i[2];
  // C[0] = LeadingEdge_ip1[1];      C[1] = LeadingEdge_ip1[2];

  // BC[0] = C[0] - B[0]; BC[1] = C[1] - B[1];
  // AB[0] = B[0] - A[0]; AB[1] = B[1] - A[1];
  // AC[0] = C[0] - A[0]; AC[1] = C[1] - A[1];
  // BC_MOD = sqrt(BC[0]*BC[0] + BC[1]*BC[1] );
  // AB_MOD = sqrt(AB[0]*AB[0] + AB[1]*AB[1] );
  // AC_MOD = sqrt(AC[0]*AC[0] + AC[1]*AC[1] );
  // AB_CROSS_AC = AB[0]* AC[1] - AB[1]* AC[0];

  // su2double Curvature_Leading = fabs(1.0/(0.5*BC_MOD*AB_MOD*AC_MOD/AB_CROSS_AC));

  A[0] = TrailingEdge_im1[1];      A[1] = TrailingEdge_im1[2];
  B[0] = TrailingEdge_i[1];                B[1] = TrailingEdge_i[2];
  C[0] = TrailingEdge_ip1[1];      C[1] = TrailingEdge_ip1[2];

  BC[0] = C[0] - B[0]; BC[1] = C[1] - B[1];
  AB[0] = B[0] - A[0]; AB[1] = B[1] - A[1];
  AC[0] = C[0] - A[0]; AC[1] = C[1] - A[1];
  BC_MOD = sqrt(BC[0]*BC[0] + BC[1]*BC[1] );
  AB_MOD = sqrt(AB[0]*AB[0] + AB[1]*AB[1] );
  AC_MOD = sqrt(AC[0]*AC[0] + AC[1]*AC[1] );
  AB_CROSS_AC = AB[0]* AC[1] - AB[1]* AC[0];

  su2double Curvature_Trailing = fabs(1.0/(0.5*BC_MOD*AB_MOD*AC_MOD/AB_CROSS_AC));

  // su2double Curvature = 0.5*(Curvature_Leading + Curvature_Trailing);

  return Curvature_Trailing;

}

su2double CPhysicalGeometry::Compute_Twist(su2double *Plane_P0, su2double *Plane_Normal, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {
  unsigned long iVertex, Trailing_Point, Leading_Point;
  su2double MaxDistance, Distance, Twist = 0.0;
  
  /*--- Find the leading and trailing edges and compute the angle of attack ---*/

  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }
  
  Twist = atan((Zcoord_Airfoil[Leading_Point] - Zcoord_Airfoil[Trailing_Point]) / (Xcoord_Airfoil[Trailing_Point] - Xcoord_Airfoil[Leading_Point]))*180/PI_NUMBER;

  return Twist;

}

void CPhysicalGeometry::Compute_Wing_LeadingTrailing(su2double *LeadingEdge, su2double *TrailingEdge, su2double *Plane_P0, su2double *Plane_Normal,
                                                vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point, Leading_Point;
  su2double MaxDistance, Distance;

  /*--- Find the leading and trailing edges and compute the angle of attack ---*/

  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {

    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));

    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }

  LeadingEdge[0] = Xcoord_Airfoil[Leading_Point];
  LeadingEdge[1] = Ycoord_Airfoil[Leading_Point];
  LeadingEdge[2] = Zcoord_Airfoil[Leading_Point];
  
  TrailingEdge[0] = Xcoord_Airfoil[Trailing_Point];
  TrailingEdge[1] = Ycoord_Airfoil[Trailing_Point];
  TrailingEdge[2] = Zcoord_Airfoil[Trailing_Point];
  
}

void CPhysicalGeometry::Compute_Fuselage_LeadingTrailing(su2double *LeadingEdge, su2double *TrailingEdge, su2double *Plane_P0, su2double *Plane_Normal,
                                                vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point, Leading_Point;
  su2double MaxDistance, Distance;

  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0));
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }

  LeadingEdge[0] = Xcoord_Airfoil[Leading_Point];
  LeadingEdge[1] = Ycoord_Airfoil[Leading_Point];
  LeadingEdge[2] = Zcoord_Airfoil[Leading_Point];

  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Zcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }

  TrailingEdge[0] = 0.5*(Xcoord_Airfoil[Trailing_Point]+Xcoord_Airfoil[Leading_Point]);
  TrailingEdge[1] = 0.5*(Ycoord_Airfoil[Trailing_Point]+Ycoord_Airfoil[Leading_Point]);
  TrailingEdge[2] = 0.5*(Zcoord_Airfoil[Trailing_Point]+Zcoord_Airfoil[Leading_Point]);

}

su2double CPhysicalGeometry::Compute_Chord(su2double *Plane_P0, su2double *Plane_Normal, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {
  unsigned long iVertex, Trailing_Point;
  su2double MaxDistance, Distance, Chord = 0.0;
  
  /*--- Find the leading and trailing edges and compute the angle of attack ---*/
  MaxDistance = 0.0; Trailing_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    
    if (MaxDistance < Distance) { MaxDistance = Distance; }
  }
  
  Chord = MaxDistance;
  
  return Chord;
  
}

su2double CPhysicalGeometry::Compute_Width(su2double *Plane_P0, su2double *Plane_Normal, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point;
  su2double MaxDistance, Distance, Width = 0.0;

  MaxDistance = 0.0; Trailing_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = fabs(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point]);
    if (MaxDistance < Distance) { MaxDistance = Distance; }
  }

  Width = MaxDistance;
  return Width;

}

su2double CPhysicalGeometry::Compute_WaterLineWidth(su2double *Plane_P0, su2double *Plane_Normal, CConfig *config, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point;
  su2double MinDistance, Distance, WaterLineWidth = 0.0;
  su2double WaterLine = config->GetGeo_Waterline_Location();

  MinDistance = 1E10; WaterLineWidth = 0; Trailing_Point = 0;
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = fabs(Zcoord_Airfoil[iVertex] - WaterLine);
    if (Distance < MinDistance) {
    	MinDistance = Distance;
    	WaterLineWidth = fabs(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point]);
    }
  }

  return WaterLineWidth;

}

su2double CPhysicalGeometry::Compute_Height(su2double *Plane_P0, su2double *Plane_Normal, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point;
  su2double MaxDistance, Distance, Height = 0.0;

  MaxDistance = 0.0; Trailing_Point = 0;
  for (iVertex = 1; iVertex < Zcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    if (MaxDistance < Distance) { MaxDistance = Distance; }
  }

  Height = MaxDistance;

  return Height;

}

su2double CPhysicalGeometry::Compute_LERadius(su2double *Plane_P0, su2double *Plane_Normal, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {

  unsigned long iVertex, Trailing_Point, Leading_Point;
  su2double MaxDistance, Distance, LERadius = 0.0, X1, X2, X3, Y1, Y2, Y3, Ma, Mb, Xc, Yc, Radius;
  
  /*--- Find the leading and trailing edges and compute the radius of curvature ---*/
  
  MaxDistance = 0.0; Trailing_Point = 0;  Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }
  
  X1 = Xcoord_Airfoil[Leading_Point-3];
  Y1 = Zcoord_Airfoil[Leading_Point-3];

  X2 = Xcoord_Airfoil[Leading_Point];
  Y2 = Zcoord_Airfoil[Leading_Point];
  
  X3 = Xcoord_Airfoil[Leading_Point+3];
  Y3 = Zcoord_Airfoil[Leading_Point+3];
  
  if (X2 != X1) Ma = (Y2-Y1) / (X2-X1); else Ma = 0.0;
  if (X3 != X2) Mb = (Y3-Y2) / (X3-X2); else Mb = 0.0;

  if (Mb != Ma) Xc = (Ma*Mb*(Y1-Y3)+Mb*(X1+X2)-Ma*(X2+X3))/(2.0*(Mb-Ma)); else Xc = 0.0;
  if (Ma != 0.0) Yc = -(1.0/Ma)*(Xc-0.5*(X1+X2))+0.5*(Y1+Y2); else Yc = 0.0;
  
  Radius = sqrt((Xc-X1)*(Xc-X1)+(Yc-Y1)*(Yc-Y1));
  if (Radius != 0.0) LERadius = 1.0/Radius; else LERadius = 0.0;

  return LERadius;
  
}

su2double CPhysicalGeometry::Compute_Thickness(su2double *Plane_P0, su2double *Plane_Normal, su2double Location, CConfig *config, vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil, su2double &ZLoc) {

  unsigned long iVertex, jVertex, n_Upper, n_Lower, Trailing_Point, Leading_Point;
  su2double Thickness_Location, Normal[3], Tangent[3], BiNormal[3], auxXCoord, auxYCoord, auxZCoord, Thickness_Value = 0.0, Length, Xcoord_Trailing, Ycoord_Trailing, Zcoord_Trailing, ValCos, ValSin, XValue, ZValue, zp1, zpn, Chord, MaxDistance, Distance, AoA;
  vector<su2double> Xcoord_Upper, Ycoord_Upper, Zcoord_Upper, Z2coord_Upper, Xcoord_Lower, Ycoord_Lower, Zcoord_Lower, Z2coord_Lower, Z2coord, Xcoord_Normal, Ycoord_Normal, Zcoord_Normal, Xcoord_Airfoil_, Ycoord_Airfoil_, Zcoord_Airfoil_;
  su2double Zcoord_Up, Zcoord_Down, ZLoc_, YLoc_;
  
  /*--- Find the leading and trailing edges and compute the angle of attack ---*/
  
  MaxDistance = 0.0; Trailing_Point = 0; Leading_Point = 0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Distance = sqrt(pow(Xcoord_Airfoil[iVertex] - Xcoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Ycoord_Airfoil[iVertex] - Ycoord_Airfoil[Trailing_Point], 2.0) +
                    pow(Zcoord_Airfoil[iVertex] - Zcoord_Airfoil[Trailing_Point], 2.0));
    
    if (MaxDistance < Distance) { MaxDistance = Distance; Leading_Point = iVertex; }
  }
  
  AoA = atan((Zcoord_Airfoil[Leading_Point] - Zcoord_Airfoil[Trailing_Point]) / (Xcoord_Airfoil[Trailing_Point] - Xcoord_Airfoil[Leading_Point]))*180/PI_NUMBER;
  Chord = MaxDistance;
  
  /*--- Translate to the origin ---*/
  
  Xcoord_Trailing = Xcoord_Airfoil[0];
  Ycoord_Trailing = Ycoord_Airfoil[0];
  Zcoord_Trailing = Zcoord_Airfoil[0];
  
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    Xcoord_Airfoil_.push_back(Xcoord_Airfoil[iVertex] - Xcoord_Trailing);
    Ycoord_Airfoil_.push_back(Ycoord_Airfoil[iVertex] - Ycoord_Trailing);
    Zcoord_Airfoil_.push_back(Zcoord_Airfoil[iVertex] - Zcoord_Trailing);
  }
  
  /*--- Rotate the airfoil ---*/
  
  ValCos = cos(AoA*PI_NUMBER/180.0);
  ValSin = sin(AoA*PI_NUMBER/180.0);
  
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size(); iVertex++) {
    XValue = Xcoord_Airfoil_[iVertex];
    ZValue = Zcoord_Airfoil_[iVertex];
    
    Xcoord_Airfoil_[iVertex] = XValue*ValCos - ZValue*ValSin;
    Zcoord_Airfoil_[iVertex] = ZValue*ValCos + XValue*ValSin;
  }
  
  /*--- Identify upper and lower side, and store the value of the normal --*/
  
  for (iVertex = 1; iVertex < Xcoord_Airfoil_.size(); iVertex++) {
    Tangent[0] = Xcoord_Airfoil_[iVertex] - Xcoord_Airfoil_[iVertex-1];
    Tangent[1] = Ycoord_Airfoil_[iVertex] - Ycoord_Airfoil_[iVertex-1];
    Tangent[2] = Zcoord_Airfoil_[iVertex] - Zcoord_Airfoil_[iVertex-1];
    Length = sqrt(pow(Tangent[0], 2.0) + pow(Tangent[1], 2.0) + pow(Tangent[2], 2.0));
    Tangent[0] /= Length; Tangent[1] /= Length; Tangent[2] /= Length;
    
    BiNormal[0] = Plane_Normal[0];
    BiNormal[1] = Plane_Normal[1];
    BiNormal[2] = Plane_Normal[2];
    Length = sqrt(pow(BiNormal[0], 2.0) + pow(BiNormal[1], 2.0) + pow(BiNormal[2], 2.0));
    BiNormal[0] /= Length; BiNormal[1] /= Length; BiNormal[2] /= Length;
    
    Normal[0] = Tangent[1]*BiNormal[2] - Tangent[2]*BiNormal[1];
    Normal[1] = Tangent[2]*BiNormal[0] - Tangent[0]*BiNormal[2];
    Normal[2] = Tangent[0]*BiNormal[1] - Tangent[1]*BiNormal[0];
    
    Xcoord_Normal.push_back(Normal[0]); Ycoord_Normal.push_back(Normal[1]); Zcoord_Normal.push_back(Normal[2]);
    
    unsigned short index = 2;
    
    if (Normal[index] >= 0.0) {
      Xcoord_Upper.push_back(Xcoord_Airfoil_[iVertex]);
      Ycoord_Upper.push_back(Ycoord_Airfoil_[iVertex]);
      Zcoord_Upper.push_back(Zcoord_Airfoil_[iVertex]);
    }
    else {
      Xcoord_Lower.push_back(Xcoord_Airfoil_[iVertex]);
      Ycoord_Lower.push_back(Ycoord_Airfoil_[iVertex]);
      Zcoord_Lower.push_back(Zcoord_Airfoil_[iVertex]);
    }
    
  }
  
  /*--- Order the arrays using the X component ---*/
  
  for (iVertex = 0; iVertex < Xcoord_Upper.size(); iVertex++) {
    for (jVertex = 0; jVertex < Xcoord_Upper.size() - 1 - iVertex; jVertex++) {
      if (Xcoord_Upper[jVertex] > Xcoord_Upper[jVertex+1]) {
        auxXCoord = Xcoord_Upper[jVertex]; Xcoord_Upper[jVertex] = Xcoord_Upper[jVertex+1]; Xcoord_Upper[jVertex+1] = auxXCoord;
        auxYCoord = Ycoord_Upper[jVertex]; Ycoord_Upper[jVertex] = Ycoord_Upper[jVertex+1]; Ycoord_Upper[jVertex+1] = auxYCoord;
        auxZCoord = Zcoord_Upper[jVertex]; Zcoord_Upper[jVertex] = Zcoord_Upper[jVertex+1]; Zcoord_Upper[jVertex+1] = auxZCoord;
      }
    }
  }
  
  /*--- Order the arrays using the X component ---*/
  
  for (iVertex = 0; iVertex < Xcoord_Lower.size(); iVertex++) {
    for (jVertex = 0; jVertex < Xcoord_Lower.size() - 1 - iVertex; jVertex++) {
      if (Xcoord_Lower[jVertex] > Xcoord_Lower[jVertex+1]) {
        auxXCoord = Xcoord_Lower[jVertex]; Xcoord_Lower[jVertex] = Xcoord_Lower[jVertex+1]; Xcoord_Lower[jVertex+1] = auxXCoord;
        auxYCoord = Ycoord_Lower[jVertex]; Ycoord_Lower[jVertex] = Ycoord_Lower[jVertex+1]; Ycoord_Lower[jVertex+1] = auxYCoord;
        auxZCoord = Zcoord_Lower[jVertex]; Zcoord_Lower[jVertex] = Zcoord_Lower[jVertex+1]; Zcoord_Lower[jVertex+1] = auxZCoord;
      }
    }
  }
  
  n_Upper = Xcoord_Upper.size();
  if (n_Upper > 1) {
    zp1 = (Zcoord_Upper[1]-Zcoord_Upper[0])/(Xcoord_Upper[1]-Xcoord_Upper[0]);
    zpn = (Zcoord_Upper[n_Upper-1]-Zcoord_Upper[n_Upper-2])/(Xcoord_Upper[n_Upper-1]-Xcoord_Upper[n_Upper-2]);
    Z2coord_Upper.resize(n_Upper+1);
    SetSpline(Xcoord_Upper, Zcoord_Upper, n_Upper, zp1, zpn, Z2coord_Upper);
  }
  
  n_Lower = Xcoord_Lower.size();
  if (n_Lower > 1) {
    zp1 = (Zcoord_Lower[1]-Zcoord_Lower[0])/(Xcoord_Lower[1]-Xcoord_Lower[0]);
    zpn = (Zcoord_Lower[n_Lower-1]-Zcoord_Lower[n_Lower-2])/(Xcoord_Lower[n_Lower-1]-Xcoord_Lower[n_Lower-2]);
    Z2coord_Lower.resize(n_Lower+1);
    SetSpline(Xcoord_Lower, Zcoord_Lower, n_Lower, zp1, zpn, Z2coord_Lower);
  }
  
  if ((n_Upper > 1) && (n_Lower > 1)) {
    
    Thickness_Location = - Chord*(1.0-Location);
    
    Zcoord_Up = GetSpline(Xcoord_Upper, Zcoord_Upper, Z2coord_Upper, n_Upper, Thickness_Location);
    Zcoord_Down = GetSpline(Xcoord_Lower, Zcoord_Lower, Z2coord_Lower, n_Lower, Thickness_Location);
    
    YLoc_ = Thickness_Location;
    ZLoc_ = 0.5*(Zcoord_Up + Zcoord_Down);
    
    ZLoc = sin(-AoA*PI_NUMBER/180.0)*YLoc_ + cos(-AoA*PI_NUMBER/180.0)*ZLoc_ + Zcoord_Trailing;
    
    /*--- Compute the thickness (we add a fabs because we can not guarantee the
     right sorting of the points and the upper and/or lower part of the airfoil is not well defined) ---*/
    
    Thickness_Value = fabs(Zcoord_Up - Zcoord_Down);
    
  }
  else { Thickness_Value = 0.0; }

  return Thickness_Value;
  
}

su2double CPhysicalGeometry::Compute_Area(su2double *Plane_P0, su2double *Plane_Normal, CConfig *config,
                                          vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {
  unsigned long iVertex;
  su2double Area_Value = 0.0;
  vector<su2double> Xcoord_Upper, Ycoord_Upper, Zcoord_Upper, Xcoord_Lower, Ycoord_Lower, Zcoord_Lower, Z2coord, Xcoord_Normal, Ycoord_Normal, Zcoord_Normal, Xcoord_Airfoil_, Ycoord_Airfoil_, Zcoord_Airfoil_;
  su2double DeltaZ, DeltaX, X, Z;
  
  /*--- Use the Green theorem to evaluate the area (the points have been sortered),
   we assume that the airfoil is in the X-Z plane  ---*/
  
  Area_Value = 0.0;
  
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size()-1; iVertex++) {
    X = 0.5*(Xcoord_Airfoil[iVertex]+Xcoord_Airfoil[iVertex+1]);
    Z = 0.5*(Zcoord_Airfoil[iVertex]+Zcoord_Airfoil[iVertex+1]);
    DeltaX = Xcoord_Airfoil[iVertex+1] - Xcoord_Airfoil[iVertex];
    DeltaZ = Zcoord_Airfoil[iVertex+1] - Zcoord_Airfoil[iVertex];
    Area_Value += 0.5*( X*DeltaZ-Z*DeltaX);
  }
  
  X = 0.5*(Xcoord_Airfoil[Xcoord_Airfoil.size()-1]+Xcoord_Airfoil[0]);
  Z = 0.5*(Zcoord_Airfoil[Xcoord_Airfoil.size()-1]+Zcoord_Airfoil[0]);
  DeltaX = Xcoord_Airfoil[0] - Xcoord_Airfoil[Xcoord_Airfoil.size()-1];
  DeltaZ = Zcoord_Airfoil[0] - Zcoord_Airfoil[Xcoord_Airfoil.size()-1];
  Area_Value += 0.5 * (X*DeltaZ-Z*DeltaX);
  
  Area_Value = fabs(Area_Value);
  
  return Area_Value;
  
}

su2double CPhysicalGeometry::Compute_Length(su2double *Plane_P0, su2double *Plane_Normal, CConfig *config,
                                          vector<su2double> &Xcoord_Airfoil, vector<su2double> &Ycoord_Airfoil, vector<su2double> &Zcoord_Airfoil) {
  unsigned long iVertex;
  su2double Length_Value = 0.0, Length_Value_ = 0.0;
  su2double DeltaZ, DeltaX;

  /*--- Not that in a symmetry plane configuration there is an extra edge that connects
   the two extremes, and we really don't now the curve orientation. We will evaluate
   both distance and picked the smallest one ---*/

  Length_Value = 0.0;
  for (iVertex = 0; iVertex < Xcoord_Airfoil.size()-2; iVertex++) {
    DeltaX = Xcoord_Airfoil[iVertex+1] - Xcoord_Airfoil[iVertex];
    DeltaZ = Zcoord_Airfoil[iVertex+1] - Zcoord_Airfoil[iVertex];
    Length_Value += sqrt(DeltaX*DeltaX + DeltaZ*DeltaZ);
  }

  Length_Value_ = 0.0;
  for (iVertex = 1; iVertex < Xcoord_Airfoil.size()-1; iVertex++) {
    DeltaX = Xcoord_Airfoil[iVertex+1] - Xcoord_Airfoil[iVertex];
    DeltaZ = Zcoord_Airfoil[iVertex+1] - Zcoord_Airfoil[iVertex];
    Length_Value_ += sqrt(DeltaX*DeltaX + DeltaZ*DeltaZ);
  }

  Length_Value = min(Length_Value, Length_Value_);

  return Length_Value;

}

void CPhysicalGeometry::Compute_Wing(CConfig *config, bool original_surface,
                                     su2double &Wing_Volume, su2double &Wing_MinMaxThickness, su2double &Wing_MaxMaxThickness, su2double &Wing_MinChord, su2double &Wing_MaxChord,
                                     su2double &Wing_MinLERadius, su2double &Wing_MaxLERadius,
                                     su2double &Wing_MinToC, su2double &Wing_MaxToC, su2double &Wing_ObjFun_MinToC, su2double &Wing_MaxTwist, su2double &Wing_MaxCurvature,
                                     su2double &Wing_MaxDihedral) {

  unsigned short iPlane, iDim, nPlane = 0;
  unsigned long iVertex;
  su2double MinPlane, MaxPlane, dPlane, *Area, *MaxThickness, *ToC, *Chord, *LERadius, *Twist, *Curvature, *Dihedral, SemiSpan;
  vector<su2double> *Xcoord_Airfoil, *Ycoord_Airfoil, *Zcoord_Airfoil, *Variable_Airfoil;
  ofstream Wing_File, Section_File;
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  
  /*--- Make a large number of section cuts for approximating volume ---*/
  
  nPlane = config->GetnWingStations();
  SemiSpan = config->GetSemiSpan();
  
  /*--- Allocate memory for the section cutting ---*/
  
  Area = new su2double [nPlane];
  MaxThickness = new su2double [nPlane];
  Chord = new su2double [nPlane];
  LERadius = new su2double [nPlane];
  ToC = new su2double [nPlane];
  Twist = new su2double [nPlane];
  Curvature = new su2double [nPlane];
  Dihedral = new su2double [nPlane];

  su2double **LeadingEdge = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    LeadingEdge[iPlane] = new su2double[nDim];
  
  su2double **TrailingEdge = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    TrailingEdge[iPlane] = new su2double[nDim];

  su2double **Plane_P0 = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    Plane_P0[iPlane] = new su2double[nDim];
  
  su2double **Plane_Normal = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    Plane_Normal[iPlane] = new su2double[nDim];
  
  MinPlane = config->GetStations_Bounds(0); MaxPlane = config->GetStations_Bounds(1);
  dPlane = fabs((MaxPlane - MinPlane)/su2double(nPlane-1));

  for (iPlane = 0; iPlane < nPlane; iPlane++) {
    Plane_Normal[iPlane][0] = 0.0;    Plane_P0[iPlane][0] = 0.0;
    Plane_Normal[iPlane][1] = 0.0;    Plane_P0[iPlane][1] = 0.0;
    Plane_Normal[iPlane][2] = 0.0;    Plane_P0[iPlane][2] = 0.0;

    if (config->GetGeo_Description() == FUSELAGE) {
      Plane_Normal[iPlane][0] = 1.0;
      Plane_P0[iPlane][0] = MinPlane + iPlane*dPlane;
    }

    if (config->GetGeo_Description() == WING) {
      Plane_Normal[iPlane][1] = 1.0;
      Plane_P0[iPlane][1] = MinPlane + iPlane*dPlane;
    }

    if (config->GetGeo_Description() == TWOD_AIRFOIL) {
      Plane_Normal[iPlane][2] = 1.0;
      Plane_P0[iPlane][2] = MinPlane + iPlane*dPlane;
    }

  }
  
  /*--- Allocate some vectors for storing airfoil coordinates ---*/
  
  Xcoord_Airfoil   = new vector<su2double>[nPlane];
  Ycoord_Airfoil   = new vector<su2double>[nPlane];
  Zcoord_Airfoil   = new vector<su2double>[nPlane];
  Variable_Airfoil = new vector<su2double>[nPlane];
  
  /*--- Create the section slices through the geometry ---*/

  for (iPlane = 0; iPlane < nPlane; iPlane++) {

    ComputeAirfoil_Section(Plane_P0[iPlane], Plane_Normal[iPlane],
                           -1E6, 1E6, NULL, Xcoord_Airfoil[iPlane],
                           Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane],
                           Variable_Airfoil[iPlane], original_surface, config);

   }

  /*--- Compute airfoil characteristic only in the master node ---*/
  
  if (rank == MASTER_NODE) {
    
    /*--- Write an output file---*/

    if (config->GetOutput_FileFormat() == PARAVIEW) {
      Wing_File.open("wing_distribution.csv", ios::out);
      if (config->GetSystemMeasurements() == US)
        Wing_File << "\"yCoord/SemiSpan\",\"Area (in^2)\",\"Max. Thickness (in)\",\"Chord (in)\",\"Leading Edge Radius (1/in)\",\"Max. Thickness/Chord\",\"Twist (deg)\",\"Curvature (1/in)\",\"Dihedral (deg)\",\"Leading Edge XLoc/SemiSpan\",\"Leading Edge ZLoc/SemiSpan\",\"Trailing Edge XLoc/SemiSpan\",\"Trailing Edge ZLoc/SemiSpan\"" << endl;
      else
        Wing_File << "\"yCoord/SemiSpan\",\"Area (m^2)\",\"Max. Thickness (m)\",\"Chord (m)\",\"Leading Edge Radius (1/m)\",\"Max. Thickness/Chord\",\"Twist (deg)\",\"Curvature (1/in)\",\"Dihedral (deg)\",\"Leading Edge XLoc/SemiSpan\",\"Leading Edge ZLoc/SemiSpan\",\"Trailing Edge XLoc/SemiSpan\",\"Trailing Edge ZLoc/SemiSpan\"" << endl;
    }
    else {
      Wing_File.open("wing_description.dat", ios::out);
      Wing_File << "TITLE = \"Wing description\"" << endl;
      if (config->GetSystemMeasurements() == US)
        Wing_File << "VARIABLES = \"<greek>h</greek>\",\"Area (in<sup>2</sup>)\",\"Max. Thickness (in)\",\"Chord (in)\",\"Leading Edge Radius (1/in)\",\"Max. Thickness/Chord\",\"Twist (deg)\",\"Curvature (1/in)\",\"Dihedral (deg)\",\"Leading Edge XLoc/SemiSpan\",\"Leading Edge ZLoc/SemiSpan\",\"Trailing Edge XLoc/SemiSpan\",\"Trailing Edge ZLoc/SemiSpan\"" << endl;
      else
        Wing_File << "VARIABLES = \"<greek>h</greek>\",\"Area (m<sup>2</sup>)\",\"Max. Thickness (m)\",\"Chord (m)\",\"Leading Edge Radius (1/m)\",\"Max. Thickness/Chord\",\"Twist (deg)\",\"Curvature (1/m)\",\"Dihedral (deg)\",\"Leading Edge XLoc/SemiSpan\",\"Leading Edge ZLoc/SemiSpan\",\"Trailing Edge XLoc/SemiSpan\",\"Trailing Edge ZLoc/SemiSpan\"" << endl;
      Wing_File << "ZONE T= \"Baseline wing\"" << endl;
    }


    /*--- Evaluate  geometrical quatities that do not require any kind of filter, local to each point ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      for (iDim = 0; iDim < nDim; iDim++) {
        LeadingEdge[iPlane][iDim]  = 0.0;
        TrailingEdge[iPlane][iDim] = 0.0;
      }

      Area[iPlane]                = 0.0;
      MaxThickness[iPlane]        = 0.0;
      Chord[iPlane]               = 0.0;
      LERadius[iPlane]            = 0.0;
      ToC[iPlane]                 = 0.0;
      Twist[iPlane]               = 0.0;

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        Compute_Wing_LeadingTrailing(LeadingEdge[iPlane], TrailingEdge[iPlane], Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Area[iPlane] = Compute_Area(Plane_P0[iPlane], Plane_Normal[iPlane], config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        MaxThickness[iPlane] = Compute_MaxThickness(Plane_P0[iPlane], Plane_Normal[iPlane], config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Chord[iPlane] = Compute_Chord(Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Twist[iPlane] = Compute_Twist(Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        LERadius[iPlane] = Compute_LERadius(Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        ToC[iPlane] = MaxThickness[iPlane] / Chord[iPlane];


      }

    }

    /*--- Evaluate  geometrical quatities that have been computed using a filtered value (they depend on more than one point) ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      Curvature[iPlane]      = 0.0;
      Dihedral[iPlane]       = 0.0;

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        if ((iPlane == 0) || (iPlane == nPlane-1)) Curvature[iPlane] = 0.0;
        else Curvature[iPlane] = Compute_Curvature(LeadingEdge[iPlane-1], TrailingEdge[iPlane-1],
                                                   LeadingEdge[iPlane], TrailingEdge[iPlane],
                                                   LeadingEdge[iPlane+1], TrailingEdge[iPlane+1]);

        if (iPlane == 0) Dihedral[iPlane] = 0.0;
        else Dihedral[iPlane] = Compute_Dihedral(LeadingEdge[iPlane-1], TrailingEdge[iPlane-1],
                                                 LeadingEdge[iPlane], TrailingEdge[iPlane]);

      }

    }

    /*--- Set the curvature and dihedral angles at the extremes ---*/

    if (nPlane > 1) {
      if ((Xcoord_Airfoil[0].size() != 0) && (Xcoord_Airfoil[1].size() != 0)) {
        Curvature[0] = Curvature[1]; Dihedral[0] = Dihedral[1];
      }
      if ((Xcoord_Airfoil[nPlane-1].size() != 0) && (Xcoord_Airfoil[nPlane-2].size() != 0)) {
        Curvature[nPlane-1] = Curvature[nPlane-2];
      }
    }

    /*--- Plot the geometrical quatities ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {
    	if (Xcoord_Airfoil[iPlane].size() > 1) {
    		if (config->GetOutput_FileFormat() == PARAVIEW) {
    			Wing_File  << Ycoord_Airfoil[iPlane][0]/SemiSpan <<", "<< Area[iPlane] <<", "<< Chord[iPlane] <<", "<< LERadius[iPlane] <<", "<< ToC[iPlane]
    			           <<", "<< Twist[iPlane] <<", "<< Curvature[iPlane] <<", "<< Dihedral[iPlane]
    			           <<", "<< LeadingEdge[iPlane][0]/SemiSpan <<", "<< LeadingEdge[iPlane][2]/SemiSpan
    			           <<", "<< TrailingEdge[iPlane][0]/SemiSpan <<", "<< TrailingEdge[iPlane][2]/SemiSpan << endl;



    		}
    		else  {
    			Wing_File  << Ycoord_Airfoil[iPlane][0]/SemiSpan <<" "<< Area[iPlane] <<" "<< Chord[iPlane] <<" "<< LERadius[iPlane] <<" "<< ToC[iPlane]
    			           <<" "<< Twist[iPlane] <<" "<< Curvature[iPlane]  <<" "<< Dihedral[iPlane]
    			           <<" "<< LeadingEdge[iPlane][0]/SemiSpan <<" "<< LeadingEdge[iPlane][2]/SemiSpan
    			           <<" "<< TrailingEdge[iPlane][0]/SemiSpan <<" "<< TrailingEdge[iPlane][2]/SemiSpan << endl;

    		}
    	}
    }

    Wing_File.close();

    Section_File.open("wing_slices.dat", ios::out);

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      if (iPlane == 0) {
        Section_File << "TITLE = \"Aircraft Slices\"" << endl;
        if (config->GetSystemMeasurements() == US)
          Section_File << "VARIABLES = \"x (in)\", \"y (in)\", \"z (in)\", \"x<sub>2D</sub>/c\", \"y<sub>2D</sub>/c\"" << endl;
        else Section_File << "VARIABLES = \"x (m)\", \"y (m)\", \"z (m)\", \"x<sub>2D</sub>/c\", \"y<sub>2D</sub>/c\"" << endl;
      }

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        Section_File << "ZONE T=\"<greek>h</greek> = " << Ycoord_Airfoil[iPlane][0]/SemiSpan << " \", I= " << Xcoord_Airfoil[iPlane].size() << ", F=POINT" << endl;

        for (iVertex = 0; iVertex < Xcoord_Airfoil[iPlane].size(); iVertex++) {

          /*--- Move to the origin  ---*/

          su2double XValue_ = Xcoord_Airfoil[iPlane][iVertex] - LeadingEdge[iPlane][0];
          su2double ZValue_ = Zcoord_Airfoil[iPlane][iVertex] - LeadingEdge[iPlane][2];

          /*--- Rotate the airfoil and divide by the chord ---*/

          su2double ValCos = cos(Twist[iPlane]*PI_NUMBER/180.0);
          su2double ValSin = sin(Twist[iPlane]*PI_NUMBER/180.0);

          su2double XValue = (XValue_*ValCos - ZValue_*ValSin) / Chord[iPlane];
          su2double ZValue = (ZValue_*ValCos + XValue_*ValSin) / Chord[iPlane];

          /*--- Write the file ---*/

          Section_File  << Xcoord_Airfoil[iPlane][iVertex] << " " << Ycoord_Airfoil[iPlane][iVertex] << " " << Zcoord_Airfoil[iPlane][iVertex] << " " << XValue  << " " << ZValue << endl;
        }
      }

    }

    Section_File.close();


    /*--- Compute the wing volume using a composite Simpson's rule ---*/

    Wing_Volume = 0.0;
    for (iPlane = 0; iPlane < nPlane-2; iPlane+=2) {
      if (Xcoord_Airfoil[iPlane].size() > 1) {
        Wing_Volume += (1.0/3.0)*dPlane*(Area[iPlane] + 4.0*Area[iPlane+1] + Area[iPlane+2]);
      }
    }

    /*--- Evaluate Max and Min quantities ---*/

    Wing_MaxMaxThickness = -1E6; Wing_MinMaxThickness = 1E6; Wing_MinChord = 1E6; Wing_MaxChord = -1E6;
    Wing_MinLERadius = 1E6; Wing_MaxLERadius = -1E6; Wing_MinToC = 1E6; Wing_MaxToC = -1E6;
    Wing_MaxTwist = -1E6; Wing_MaxCurvature = -1E6; Wing_MaxDihedral = -1E6;

    for (iPlane = 0; iPlane < nPlane; iPlane++) {
      if (MaxThickness[iPlane] != 0.0) Wing_MinMaxThickness = min(Wing_MinMaxThickness, MaxThickness[iPlane]);
      Wing_MaxMaxThickness = max(Wing_MaxMaxThickness, MaxThickness[iPlane]);
      if (Chord[iPlane] != 0.0) Wing_MinChord = min(Wing_MinChord, Chord[iPlane]);
      Wing_MaxChord = max(Wing_MaxChord, Chord[iPlane]);
      if (LERadius[iPlane] != 0.0) Wing_MinLERadius = min(Wing_MinLERadius, LERadius[iPlane]);
      Wing_MaxLERadius = max(Wing_MaxLERadius, LERadius[iPlane]);
      if (ToC[iPlane] != 0.0) Wing_MinToC = min(Wing_MinToC, ToC[iPlane]);
      Wing_MaxToC = max(Wing_MaxToC, ToC[iPlane]);
      Wing_ObjFun_MinToC = sqrt((Wing_MinToC - 0.07)*(Wing_MinToC - 0.07));
      Wing_MaxTwist = max(Wing_MaxTwist, fabs(Twist[iPlane]));
      Wing_MaxCurvature = max(Wing_MaxCurvature, Curvature[iPlane]);
      Wing_MaxDihedral = max(Wing_MaxDihedral, fabs(Dihedral[iPlane]));
    }

  }

  /*--- Free memory for the section cuts ---*/

  delete [] Xcoord_Airfoil;
  delete [] Ycoord_Airfoil;
  delete [] Zcoord_Airfoil;
  delete [] Variable_Airfoil;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] LeadingEdge[iPlane];
  delete [] LeadingEdge;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] TrailingEdge[iPlane];
  delete [] TrailingEdge;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] Plane_P0[iPlane];
  delete [] Plane_P0;
  
  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] Plane_Normal[iPlane];
  delete [] Plane_Normal;
  
  delete [] Area;
  delete [] MaxThickness;
  delete [] Chord;
  delete [] LERadius;
  delete [] ToC;
  delete [] Twist;
  delete [] Curvature;
  delete [] Dihedral;

}

void CPhysicalGeometry::Compute_Fuselage(CConfig *config, bool original_surface,
                                         su2double &Fuselage_Volume, su2double &Fuselage_WettedArea,
                                         su2double &Fuselage_MinWidth, su2double &Fuselage_MaxWidth,
                                         su2double &Fuselage_MinWaterLineWidth, su2double &Fuselage_MaxWaterLineWidth,
                                         su2double &Fuselage_MinHeight, su2double &Fuselage_MaxHeight,
                                         su2double &Fuselage_MaxCurvature) {

  unsigned short iPlane, iDim, nPlane = 0;
  unsigned long iVertex;
  su2double MinPlane, MaxPlane, dPlane, *Area, *Length, *Width, *WaterLineWidth, *Height, *Curvature;
  vector<su2double> *Xcoord_Airfoil, *Ycoord_Airfoil, *Zcoord_Airfoil, *Variable_Airfoil;
  ofstream Fuselage_File, Section_File;

  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif


  /*--- Make a large number of section cuts for approximating volume ---*/

  nPlane = config->GetnWingStations();

  /*--- Allocate memory for the section cutting ---*/

  Area = new su2double [nPlane];
  Length = new su2double [nPlane];
  Width = new su2double [nPlane];
  WaterLineWidth = new su2double [nPlane];
  Height = new su2double [nPlane];
  Curvature = new su2double [nPlane];

  su2double **LeadingEdge = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    LeadingEdge[iPlane] = new su2double[nDim];

  su2double **TrailingEdge = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    TrailingEdge[iPlane] = new su2double[nDim];

  su2double **Plane_P0 = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    Plane_P0[iPlane] = new su2double[nDim];

  su2double **Plane_Normal = new su2double*[nPlane];
  for (iPlane = 0; iPlane < nPlane; iPlane++ )
    Plane_Normal[iPlane] = new su2double[nDim];

  MinPlane = config->GetStations_Bounds(0); MaxPlane = config->GetStations_Bounds(1);
  dPlane = fabs((MaxPlane - MinPlane)/su2double(nPlane-1));

  for (iPlane = 0; iPlane < nPlane; iPlane++) {
    Plane_Normal[iPlane][0] = 0.0;    Plane_P0[iPlane][0] = 0.0;
    Plane_Normal[iPlane][1] = 0.0;    Plane_P0[iPlane][1] = 0.0;
    Plane_Normal[iPlane][2] = 0.0;    Plane_P0[iPlane][2] = 0.0;

    if (config->GetGeo_Description() == FUSELAGE) {
      Plane_Normal[iPlane][0] = 1.0;
      Plane_P0[iPlane][0] = MinPlane + iPlane*dPlane;
    }

    if (config->GetGeo_Description() == WING) {
      Plane_Normal[iPlane][1] = 1.0;
      Plane_P0[iPlane][1] = MinPlane + iPlane*dPlane;
    }

    if (config->GetGeo_Description() == TWOD_AIRFOIL) {
      Plane_Normal[iPlane][2] = 1.0;
      Plane_P0[iPlane][2] = MinPlane + iPlane*dPlane;
    }

  }

  /*--- Allocate some vectors for storing airfoil coordinates ---*/

  Xcoord_Airfoil   = new vector<su2double>[nPlane];
  Ycoord_Airfoil   = new vector<su2double>[nPlane];
  Zcoord_Airfoil   = new vector<su2double>[nPlane];
  Variable_Airfoil = new vector<su2double>[nPlane];

  /*--- Create the section slices through the geometry ---*/

  for (iPlane = 0; iPlane < nPlane; iPlane++) {

    ComputeAirfoil_Section(Plane_P0[iPlane], Plane_Normal[iPlane],
                           -1E6, 1E6, NULL, Xcoord_Airfoil[iPlane],
                           Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane],
                           Variable_Airfoil[iPlane], original_surface, config);
  }

  /*--- Compute the area at each section ---*/

  if (rank == MASTER_NODE) {

    /*--- Write an output file---*/

    if (config->GetOutput_FileFormat() == PARAVIEW) {
      Fuselage_File.open("fuselage_description.csv", ios::out);
      if (config->GetSystemMeasurements() == US)
        Fuselage_File << "\"x (in)\",\"Area (in^2)\",\"Length (in)\",\"Width (in)\",\"Waterline width (in)\",\"Height (in)\",\"Curvature (1/in)\",\"Generatrix Curve X (in)\",\"Generatrix Curve Y (in)\",\"Generatrix Curve Z (in)\",\"Axis Curve X (in)\",\"Axis Curve Y (in)\",\"Axis Curve Z (in)\"" << endl;
      else
        Fuselage_File << "\"x (m)\",\"Area (m^2)\",\"Length (m)\",\"Width (m)\",\"Waterline width (m)\",\"Height (m)\",\"Curvature (1/in)\",\"Generatrix Curve X (m)\",\"Generatrix Curve Y (m)\",\"Generatrix Curve Z (m)\",\"Axis Curve X (m)\",\"Axis Curve Y (m)\",\"Axis Curve Z (m)\"" << endl;
    }
    else {
      Fuselage_File.open("fuselage_description.dat", ios::out);
      Fuselage_File << "TITLE = \"Fuselage description\"" << endl;
      if (config->GetSystemMeasurements() == US)
        Fuselage_File << "VARIABLES = \"x (in)\",\"Area (in<sup>2</sup>)\",\"Length (in)\",\"Width (in)\",\"Waterline width (in)\",\"Height (in)\",\"Curvature (1/in)\",\"Generatrix Curve X (in)\",\"Generatrix Curve Y (in)\",\"Generatrix Curve Z (in)\",\"Axis Curve X (in)\",\"Axis Curve Y (in)\",\"Axis Curve Z (in)\"" << endl;
      else
        Fuselage_File << "VARIABLES = \"x (m)\",\"Area (m<sup>2</sup>)\",\"Length (m)\",\"Width (m)\",\"Waterline width (m)\",\"Height (m)\",\"Curvature (1/m)\",\"Generatrix Curve X (m)\",\"Generatrix Curve Y (m)\",\"Generatrix Curve Z (m)\",\"Axis Curve X (m)\",\"Axis Curve Y (m)\",\"Axis Curve Z (m)\"" << endl;
      Fuselage_File << "ZONE T= \"Baseline fuselage\"" << endl;
    }


    /*--- Evaluate  geometrical quatities that do not require any kind of filter, local to each point ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      for (iDim = 0; iDim < nDim; iDim++) {
        LeadingEdge[iPlane][iDim]  = 0.0;
        TrailingEdge[iPlane][iDim] = 0.0;
      }

      Area[iPlane]   = 0.0;
      Length[iPlane]   = 0.0;
      Width[iPlane]  = 0.0;
      WaterLineWidth[iPlane]  = 0.0;
      Height[iPlane] = 0.0;

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        Compute_Fuselage_LeadingTrailing(LeadingEdge[iPlane], TrailingEdge[iPlane], Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Area[iPlane] = Compute_Area(Plane_P0[iPlane], Plane_Normal[iPlane], config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Length[iPlane] = Compute_Length(Plane_P0[iPlane], Plane_Normal[iPlane], config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Width[iPlane] = Compute_Width(Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        WaterLineWidth[iPlane] = Compute_WaterLineWidth(Plane_P0[iPlane], Plane_Normal[iPlane], config, Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

        Height[iPlane] = Compute_Height(Plane_P0[iPlane], Plane_Normal[iPlane], Xcoord_Airfoil[iPlane], Ycoord_Airfoil[iPlane], Zcoord_Airfoil[iPlane]);

      }

    }

    /*--- Evaluate  geometrical quatities that have been computed using a filtered value (they depend on more than one point) ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      Curvature[iPlane] = 0.0;

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        if ((iPlane == 0) || (iPlane == nPlane-1)) Curvature[iPlane] = 0.0;
        else Curvature[iPlane] = Compute_Curvature(LeadingEdge[iPlane-1], TrailingEdge[iPlane-1],
                                                   LeadingEdge[iPlane], TrailingEdge[iPlane],
                                                   LeadingEdge[iPlane+1], TrailingEdge[iPlane+1]);

      }

    }

    /*--- Set the curvature and dihedral angles at the extremes ---*/

    if (nPlane > 1) {
      if ((Xcoord_Airfoil[0].size() != 0) && (Xcoord_Airfoil[1].size() != 0)) {
        Curvature[0] = Curvature[1];
      }
      if ((Xcoord_Airfoil[nPlane-1].size() != 0) && (Xcoord_Airfoil[nPlane-2].size() != 0)) {
        Curvature[nPlane-1] = Curvature[nPlane-2];
      }
    }

    /*--- Plot the geometrical quatities ---*/

    for (iPlane = 0; iPlane < nPlane; iPlane++) {
    	if (Xcoord_Airfoil[iPlane].size() > 1) {
    		if (config->GetOutput_FileFormat() == PARAVIEW) {
    			Fuselage_File  << -Ycoord_Airfoil[iPlane][0] <<", "<< Area[iPlane] <<", "<< Length[iPlane] <<", "<< Width[iPlane] <<", "<< WaterLineWidth[iPlane] <<", "<< Height[iPlane] <<", "<< Curvature[iPlane]
    			           <<", "<< -LeadingEdge[iPlane][1] <<", "<< LeadingEdge[iPlane][0]  <<", "<< LeadingEdge[iPlane][2]
    			           <<", "<< -TrailingEdge[iPlane][1] <<", "<< TrailingEdge[iPlane][0]  <<", "<< TrailingEdge[iPlane][2]  << endl;
    		}
    		else  {
    			Fuselage_File  << -Ycoord_Airfoil[iPlane][0] <<" "<< Area[iPlane] <<" "<< Length[iPlane] <<" "<< Width[iPlane] <<" "<< WaterLineWidth[iPlane] <<" "<< Height[iPlane] <<" "<< Curvature[iPlane]
    			           <<" "<< -LeadingEdge[iPlane][1] <<" "<< LeadingEdge[iPlane][0]  <<" "<< LeadingEdge[iPlane][2]
    			           <<" "<< -TrailingEdge[iPlane][1] <<" "<< TrailingEdge[iPlane][0]  <<" "<< TrailingEdge[iPlane][2] << endl;
    		}
    	}
    }

    Fuselage_File.close();

    Section_File.open("fuselage_slices.dat", ios::out);

    for (iPlane = 0; iPlane < nPlane; iPlane++) {

      if (iPlane == 0) {
        Section_File << "TITLE = \"Aircraft Slices\"" << endl;
        if (config->GetSystemMeasurements() == US)
          Section_File << "VARIABLES = \"x (in)\", \"y (in)\", \"z (in)\"" << endl;
        else Section_File << "VARIABLES = \"x (m)\", \"y (m)\", \"z (m)\"" << endl;
      }

      if (Xcoord_Airfoil[iPlane].size() > 1) {

        Section_File << "ZONE T=\"X = " << -Ycoord_Airfoil[iPlane][0] << " \", I= " << Ycoord_Airfoil[iPlane].size() << ", F=POINT" << endl;

        for (iVertex = 0; iVertex < Xcoord_Airfoil[iPlane].size(); iVertex++) {

          /*--- Write the file ---*/

          Section_File  << -Ycoord_Airfoil[iPlane][iVertex] << " " << Xcoord_Airfoil[iPlane][iVertex] << " " << Zcoord_Airfoil[iPlane][iVertex] << endl;
        }
      }

    }

    Section_File.close();


    /*--- Compute the fuselage volume using a composite Simpson's rule ---*/

    Fuselage_Volume = 0.0;
    for (iPlane = 0; iPlane < nPlane-2; iPlane+=2) {
      if (Xcoord_Airfoil[iPlane].size() > 1) {
      	Fuselage_Volume += (1.0/3.0)*dPlane*(Area[iPlane] + 4.0*Area[iPlane+1] + Area[iPlane+2]);
      }
    }

    /*--- Compute the fuselage wetted area ---*/

    Fuselage_WettedArea = 0.0;
    if (Xcoord_Airfoil[0].size() > 1) Fuselage_WettedArea += (1.0/2.0)*dPlane*Length[0];
    for (iPlane = 1; iPlane < nPlane-1; iPlane++) {
      if (Xcoord_Airfoil[iPlane].size() > 1) {
      	Fuselage_WettedArea += dPlane*Length[iPlane];
      }
    }
    if (Xcoord_Airfoil[nPlane-1].size() > 1) Fuselage_WettedArea += (1.0/2.0)*dPlane*Length[nPlane-1];

    /*--- Evaluate Max and Min quantities ---*/

    Fuselage_MaxWidth = -1E6; Fuselage_MinWidth = 1E6;
    Fuselage_MaxWaterLineWidth = -1E6; Fuselage_MinWaterLineWidth = 1E6;
    Fuselage_MaxHeight = -1E6; Fuselage_MinHeight = 1E6;
    Fuselage_MaxCurvature = -1E6;

    for (iPlane = 0; iPlane < nPlane; iPlane++) {
      if (Width[iPlane] != 0.0) Fuselage_MinWidth = min(Fuselage_MinWidth, Width[iPlane]);
      Fuselage_MaxWidth = max(Fuselage_MaxWidth, Width[iPlane]);
      if (WaterLineWidth[iPlane] != 0.0) Fuselage_MinWaterLineWidth = min(Fuselage_MinWaterLineWidth, WaterLineWidth[iPlane]);
      Fuselage_MaxWaterLineWidth = max(Fuselage_MaxWaterLineWidth, WaterLineWidth[iPlane]);
      if (Height[iPlane] != 0.0) Fuselage_MinHeight = min(Fuselage_MinHeight, Height[iPlane]);
      Fuselage_MaxHeight = max(Fuselage_MaxHeight, Height[iPlane]);
      Fuselage_MaxCurvature = max(Fuselage_MaxCurvature, Curvature[iPlane]);
    }

  }

  /*--- Free memory for the section cuts ---*/

  delete [] Xcoord_Airfoil;
  delete [] Ycoord_Airfoil;
  delete [] Zcoord_Airfoil;
  delete [] Variable_Airfoil;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] LeadingEdge[iPlane];
  delete [] LeadingEdge;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] TrailingEdge[iPlane];
  delete [] TrailingEdge;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] Plane_P0[iPlane];
  delete [] Plane_P0;

  for (iPlane = 0; iPlane < nPlane; iPlane++)
    delete [] Plane_Normal[iPlane];
  delete [] Plane_Normal;

  delete [] Area;
  delete [] Length;
  delete [] Width;
  delete [] WaterLineWidth;
  delete [] Height;
  delete [] Curvature;

}

CMultiGridGeometry::CMultiGridGeometry(CGeometry ***geometry, CConfig **config_container, unsigned short iMesh, unsigned short iZone) : CGeometry() {
  
  /*--- CGeometry & CConfig pointers to the fine grid level for clarity. We may
   need access to the other zones in the mesh for zone boundaries. ---*/
  
  CGeometry *fine_grid = geometry[iZone][iMesh-1];
  CConfig *config = config_container[iZone];
  
  /*--- Local variables ---*/
  
  unsigned long iPoint, Index_CoarseCV, CVPoint, iElem, iVertex, jPoint, iteration, nVertexS, nVertexR, nBufferS_Vector, nBufferR_Vector, iParent, jVertex, *Buffer_Receive_Parent = NULL, *Buffer_Send_Parent = NULL, *Buffer_Receive_Children = NULL, *Buffer_Send_Children = NULL, *Parent_Remote = NULL, *Children_Remote = NULL, *Parent_Local = NULL, *Children_Local = NULL, Local_nPointCoarse, Local_nPointFine, Global_nPointCoarse, Global_nPointFine;;
  short marker_seed;
  bool agglomerate_seed = true;
  unsigned short nChildren, iNode, counter, iMarker, jMarker, priority, MarkerS, MarkerR, *nChildren_MPI;
  vector<unsigned long> Suitable_Indirect_Neighbors, Aux_Parent;
  vector<unsigned long>::iterator it;
  int rank;

  unsigned short nMarker_Max = config->GetnMarker_Max();

  unsigned short *copy_marker = new unsigned short [nMarker_Max];
  
#ifndef HAVE_MPI
  rank = MASTER_NODE;
#else
  int send_to, receive_from;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status status;
#endif
  
  nDim = fine_grid->GetnDim(); // Write the number of dimensions of the coarse grid.
  
  /*--- Create a queue system to deo the agglomeration
   1st) More than two markers ---> Vertices (never agglomerate)
   2nd) Two markers ---> Edges (agglomerate if same BC, never agglomerate if different BC)
   3rd) One marker ---> Surface (always agglomarate)
   4th) No marker ---> Internal Volume (always agglomarate) ---*/
  
  /*--- Set a marker to indicate indirect agglomeration ---*/
  
  if (iMesh == MESH_1) {
    
    for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint ++)
      fine_grid->node[iPoint]->SetAgglomerate_Indirect(false);
    
    for (iElem = 0; iElem < fine_grid->GetnElem(); iElem++) {
      if ((fine_grid->elem[iElem]->GetVTK_Type() == HEXAHEDRON) ||
          (fine_grid->elem[iElem]->GetVTK_Type() == QUADRILATERAL)) {
        for (iNode = 0; iNode < fine_grid->elem[iElem]->GetnNodes(); iNode++) {
          iPoint = fine_grid->elem[iElem]->GetNode(iNode);
          fine_grid->node[iPoint]->SetAgglomerate_Indirect(true);
        }
      }
    }
    
  }
  
  /*--- Create the coarse grid structure using as baseline the fine grid ---*/
  
  CMultiGridQueue MGQueue_InnerCV(fine_grid->GetnPoint());
 
  nPointNode = fine_grid->GetnPoint(); 
  node = new CPoint*[fine_grid->GetnPoint()];
  for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint ++) {
    
    /*--- Create node structure ---*/
    
    node[iPoint] = new CPoint(nDim, iPoint, config);
    
    /*--- Set the indirect agglomeration to false ---*/
    
    node[iPoint]->SetAgglomerate_Indirect(false);
  }
  
  Index_CoarseCV = 0;
  
  /*--- The first step is the boundary agglomeration. ---*/
  
  for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker++) {
    
    for (iVertex = 0; iVertex < fine_grid->GetnVertex(iMarker); iVertex++) {
      iPoint = fine_grid->vertex[iMarker][iVertex]->GetNode();
      
      /*--- If the element has not being previously agglomerated and it belongs
       to the physical domain, then the agglomeration is studied ---*/
      
      if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
          (fine_grid->node[iPoint]->GetDomain()) &&
          (GeometricalCheck(iPoint, fine_grid, config))) {
        
        nChildren = 1;
        
        /*--- We set an index for the parent control volume ---*/
        
        fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
        
        /*--- We add the seed point (child) to the parent control volume ---*/
        
        node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
        agglomerate_seed = true; counter = 0; marker_seed = iMarker;
        
        /*--- For a particular point in the fine grid we save all the markers
         that are in that point ---*/
        
        for (jMarker = 0; jMarker < fine_grid->GetnMarker(); jMarker ++)
          if (fine_grid->node[iPoint]->GetVertex(jMarker) != -1) {
            copy_marker[counter] = jMarker;
            counter++;
          }
        
        /*--- To aglomerate a vertex it must have only one physical bc!!
         This can be improved. If there is only a marker, it is a good
         candidate for agglomeration ---*/
        
        if (counter == 1) agglomerate_seed = true;
        
        /*--- If there are two markers, we will aglomerate if one of the
         marker is SEND_RECEIVE ---*/
        
        if (counter == 2) {
          if ((config->GetMarker_All_KindBC(copy_marker[0]) == SEND_RECEIVE) ||
              (config->GetMarker_All_KindBC(copy_marker[1]) == SEND_RECEIVE)) agglomerate_seed = true;
          else agglomerate_seed = false;
        }
        
        /*--- If there are more than 2 markers, the aglomeration will be discarted ---*/
        
        if (counter > 2) agglomerate_seed = false;
        
        /*--- If the seed can be agglomerated, we try to agglomerate more points ---*/
        
        if (agglomerate_seed) {
          
          /*--- Now we do a sweep over all the nodes that surround the seed point ---*/
          
          for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
            
            CVPoint = fine_grid->node[iPoint]->GetPoint(iNode);
            
            /*--- The new point can be agglomerated ---*/
            
            if (SetBoundAgglomeration(CVPoint, marker_seed, fine_grid, config)) {
              
              /*--- We set the value of the parent ---*/
              
              fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);
              
              /*--- We set the value of the child ---*/
              
              node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
              nChildren++;
            }
            
          }
          
          Suitable_Indirect_Neighbors.clear();
          
          if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
            SetSuitableNeighbors(&Suitable_Indirect_Neighbors, iPoint, Index_CoarseCV, fine_grid);
          
          /*--- Now we do a sweep over all the indirect nodes that can be added ---*/
          
          for (iNode = 0; iNode <	Suitable_Indirect_Neighbors.size(); iNode ++) {
            
            CVPoint = Suitable_Indirect_Neighbors[iNode];
            
            /*--- The new point can be agglomerated ---*/
            
            if (SetBoundAgglomeration(CVPoint, marker_seed, fine_grid, config)) {
              
              /*--- We set the value of the parent ---*/
              
              fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);
              
              /*--- We set the indirect agglomeration information ---*/
              
              if (fine_grid->node[CVPoint]->GetAgglomerate_Indirect())
                node[Index_CoarseCV]->SetAgglomerate_Indirect(true);
              
              /*--- We set the value of the child ---*/
              
              node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
              nChildren++;
            }
          }
          
          
        }
        
        /*--- Update the number of child of the control volume ---*/
        
        node[Index_CoarseCV]->SetnChildren_CV(nChildren);
        Index_CoarseCV++;
      }
    }
  }
  
  /*--- Agglomerate all the nodes that have more than one physical boundary condition,
   Maybe here we can add the posibility of merging the vertex that have the same number,
   and kind  of markers---*/
  
  for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker++)
    for (iVertex = 0; iVertex < fine_grid->GetnVertex(iMarker); iVertex++) {
      iPoint = fine_grid->vertex[iMarker][iVertex]->GetNode();
      if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
          (fine_grid->node[iPoint]->GetDomain())) {
        fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
        node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
        node[Index_CoarseCV]->SetnChildren_CV(1);
        Index_CoarseCV++;
      }
    }
  
  /*--- Update the queue with the results from the boundary agglomeration ---*/
  
  for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint ++) {
    
    /*--- The CV has been agglomerated, remove form the list ---*/
    
    if (fine_grid->node[iPoint]->GetAgglomerate() == true) {
      
      MGQueue_InnerCV.RemoveCV(iPoint);
      
    }
    
    else {
      
      /*--- Count the number of agglomerated neighbors, and modify the queue ---*/
      
      priority = 0;
      for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
        jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
        if (fine_grid->node[jPoint]->GetAgglomerate() == true) priority++;
      }
      MGQueue_InnerCV.MoveCV(iPoint, priority);
    }
  }
  
  /*--- Agglomerate the domain nodes ---*/
  
  iteration = 0;
  while (!MGQueue_InnerCV.EmptyQueue() && (iteration < fine_grid->GetnPoint())) {
    
    iPoint = MGQueue_InnerCV.NextCV();
    iteration ++;
    
    /*--- If the element has not being previously agglomerated, belongs to the physical domain,
     and satisfies several geometrical criteria then the seed CV is acepted for agglomeration ---*/
    
    if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
        (fine_grid->node[iPoint]->GetDomain()) &&
        (GeometricalCheck(iPoint, fine_grid, config))) {
      
      nChildren = 1;
      
      /*--- We set an index for the parent control volume ---*/
      
      fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
      
      /*--- We add the seed point (child) to the parent control volume ---*/
      
      node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
      
      /*--- Update the queue with the seed point (remove the seed and
       increase the priority of the neighbors) ---*/
      
      MGQueue_InnerCV.Update(iPoint, fine_grid);
      
      /*--- Now we do a sweep over all the nodes that surround the seed point ---*/
      
      for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
        
        CVPoint = fine_grid->node[iPoint]->GetPoint(iNode);
        
        /*--- Determine if the CVPoint can be agglomerated ---*/
        
        if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
            (fine_grid->node[CVPoint]->GetDomain()) &&
            (GeometricalCheck(CVPoint, fine_grid, config))) {
          
          /*--- We set the value of the parent ---*/
          
          fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);
          
          /*--- We set the value of the child ---*/
          
          node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
          nChildren++;
          
          /*--- Update the queue with the new control volume (remove the CV and
           increase the priority of the neighbors) ---*/
          
          MGQueue_InnerCV.Update(CVPoint, fine_grid);
          
        }
        
      }
      
      /*--- Subrotuine to identify the indirect neighbors ---*/
      
      Suitable_Indirect_Neighbors.clear();
      if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
        SetSuitableNeighbors(&Suitable_Indirect_Neighbors, iPoint, Index_CoarseCV, fine_grid);
      
      /*--- Now we do a sweep over all the indirect nodes that can be added ---*/
      
      for (iNode = 0; iNode <	Suitable_Indirect_Neighbors.size(); iNode ++) {
        
        CVPoint = Suitable_Indirect_Neighbors[iNode];
        
        /*--- The new point can be agglomerated ---*/
        
        if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
            (fine_grid->node[CVPoint]->GetDomain())) {
          
          /*--- We set the value of the parent ---*/
          
          fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);
          
          /*--- We set the indirect agglomeration information ---*/
          
          if (fine_grid->node[CVPoint]->GetAgglomerate_Indirect())
            node[Index_CoarseCV]->SetAgglomerate_Indirect(true);
          
          /*--- We set the value of the child ---*/
          
          node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
          nChildren++;
          
          /*--- Update the queue with the new control volume (remove the CV and
           increase the priority of the neighbors) ---*/
          
          MGQueue_InnerCV.Update(CVPoint, fine_grid);
          
        }
      }
      
      /*--- Update the number of control of childrens ---*/
      
      node[Index_CoarseCV]->SetnChildren_CV(nChildren);
      Index_CoarseCV++;
    }
    else {
      
      /*--- The seed point can not be agglomerated because of size, domain, streching, etc.
       move the point to the lowest priority ---*/
      
      MGQueue_InnerCV.MoveCV(iPoint, -1);
    }
    
  }
  
  /*--- Add all the elements that have not being agglomerated, in the previous stage ---*/
  
  for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint ++) {
    if ((fine_grid->node[iPoint]->GetAgglomerate() == false) && (fine_grid->node[iPoint]->GetDomain())) {
      
      nChildren = 1;
      fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
      if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
        node[Index_CoarseCV]->SetAgglomerate_Indirect(true);
      node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
      node[Index_CoarseCV]->SetnChildren_CV(nChildren);
      Index_CoarseCV++;
      
    }
  }
  
  nPointDomain = Index_CoarseCV;
  
  /*--- Check that there are no hanging nodes ---*/
  
  unsigned long iFinePoint, iFinePoint_Neighbor, iCoarsePoint, iCoarsePoint_Complete;
  unsigned short iChildren;
  
  /*--- Find the point surrounding a point ---*/
  
  for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint ++) {
    for (iChildren = 0; iChildren <  node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
      iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
      for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode ++) {
        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
        if (iParent != iCoarsePoint) node[iCoarsePoint]->SetPoint(iParent);
      }
    }
  }
  
  /*--- Detect isolated points and merge them with its correct neighbor ---*/
  
  for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint ++) {
    
    if (node[iCoarsePoint]->GetnPoint() == 1) {
      
      /*--- Find the neighbor of the isolated point. This neighbor is the right control volume ---*/
      
      iCoarsePoint_Complete = node[iCoarsePoint]->GetPoint(0);
      
      /*--- Add the children to the connected control volume (and modify it parent indexing).
       Identify the child CV from the finest grid and added to the correct control volume.
       Set the parent CV of iFinePoint. Instead of using the original
       (iCoarsePoint) one use the new one (iCoarsePoint_Complete) ---*/
      
      nChildren = node[iCoarsePoint_Complete]->GetnChildren_CV();
      
      for (iChildren = 0; iChildren <  node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
        node[iCoarsePoint_Complete]->SetChildren_CV(nChildren, iFinePoint);
        nChildren++;
        fine_grid->node[iFinePoint]->SetParent_CV(iCoarsePoint_Complete);
      }
      
      /*--- Update the number of children control volumes ---*/
      
      node[iCoarsePoint_Complete]->SetnChildren_CV(nChildren);
      node[iCoarsePoint]->SetnChildren_CV(0);
      
    }
  }
  
  //  unsigned long iPointFree = nPointDomain-1;
  //  iCoarsePoint = 0;
  //
  //  do {
  //
  //    if (node[iCoarsePoint]->GetnChildren_CV() == 0) {
  //
  //      while (node[iPointFree]->GetnChildren_CV() == 0) {
  //        Index_CoarseCV--;
  //        iPointFree--;
  //      }
  //
  //      nChildren = node[iPointFree]->GetnChildren_CV();
  //      for (iChildren = 0; iChildren <  nChildren; iChildren ++) {
  //        iFinePoint = node[iPointFree]->GetChildren_CV(iChildren);
  //        node[iCoarsePoint]->SetChildren_CV(iChildren, iFinePoint);
  //        fine_grid->node[iFinePoint]->SetParent_CV(iCoarsePoint);
  //      }
  //      node[iCoarsePoint]->SetnChildren_CV(nChildren);
  //      node[iPointFree]->SetnChildren_CV(0);
  //
  //      Index_CoarseCV--;
  //      iPointFree--;
  //
  //    }
  //
  //    iCoarsePoint++;
  //
  //  } while ((iCoarsePoint-1) < Index_CoarseCV);
  //
  //  nPointDomain = Index_CoarseCV;
  
  /*--- Reset the point surrounding a point ---*/
  
  for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint ++) {
    node[iCoarsePoint]->ResetPoint();
  }
  
  /*--- Dealing with MPI parallelization, the objective is that the received nodes must be agglomerated
   in the same way as the donor nodes. Send the node agglomeration information of the donor
   (parent and children), Sending only occurs with MPI ---*/
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    if ((config->GetMarker_All_KindBC(iMarker) == SEND_RECEIVE) &&
        (config->GetMarker_All_SendRecv(iMarker) > 0)) {
      
      MarkerS = iMarker;  MarkerR = iMarker+1;
      
#ifdef HAVE_MPI
      send_to = config->GetMarker_All_SendRecv(MarkerS)-1;
      receive_from = abs(config->GetMarker_All_SendRecv(MarkerR))-1;
#endif

      nVertexS = fine_grid->nVertex[MarkerS];   nVertexR = fine_grid->nVertex[MarkerR];
      nBufferS_Vector = nVertexS;               nBufferR_Vector = nVertexR;
      
      /*--- Allocate Receive and send buffers  ---*/
      
      Buffer_Receive_Children = new unsigned long [nBufferR_Vector];
      Buffer_Send_Children = new unsigned long [nBufferS_Vector];
      
      Buffer_Receive_Parent = new unsigned long [nBufferR_Vector];
      Buffer_Send_Parent = new unsigned long [nBufferS_Vector];
      
      /*--- Copy the information that should be sended ---*/
      
      for (iVertex = 0; iVertex < nVertexS; iVertex++) {
        iPoint = fine_grid->vertex[MarkerS][iVertex]->GetNode();
        Buffer_Send_Children[iVertex] = iPoint;
        Buffer_Send_Parent[iVertex] = fine_grid->node[iPoint]->GetParent_CV();
      }
      
#ifdef HAVE_MPI
      /*--- Send/Receive information using Sendrecv ---*/
      SU2_MPI::Sendrecv(Buffer_Send_Children, nBufferS_Vector, MPI_UNSIGNED_LONG, send_to,0,
                   Buffer_Receive_Children, nBufferR_Vector, MPI_UNSIGNED_LONG, receive_from,0, MPI_COMM_WORLD, &status);
      SU2_MPI::Sendrecv(Buffer_Send_Parent, nBufferS_Vector, MPI_UNSIGNED_LONG, send_to,1,
                   Buffer_Receive_Parent, nBufferR_Vector, MPI_UNSIGNED_LONG, receive_from,1, MPI_COMM_WORLD, &status);
#else
      /*--- Receive information without MPI ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        Buffer_Receive_Children[iVertex] = Buffer_Send_Children[iVertex];
        Buffer_Receive_Parent[iVertex] = Buffer_Send_Parent[iVertex];
      }
#endif
      
      /*--- Deallocate send buffer ---*/
      
      delete [] Buffer_Send_Children;
      delete [] Buffer_Send_Parent;
      
      /*--- Create a list of the parent nodes without repeated parents ---*/
      
      Aux_Parent.clear();
      for (iVertex = 0; iVertex < nVertexR; iVertex++)
        Aux_Parent.push_back (Buffer_Receive_Parent[iVertex]);
      
      sort(Aux_Parent.begin(), Aux_Parent.end());
      it = unique(Aux_Parent.begin(), Aux_Parent.end());
      Aux_Parent.resize(it - Aux_Parent.begin());
      
      /*--- Allocate some structures ---*/
      
      Parent_Remote = new unsigned long[nVertexR];
      Children_Remote = new unsigned long[nVertexR];
      Parent_Local = new unsigned long[nVertexR];
      Children_Local = new unsigned long[nVertexR];
      
      /*--- Create the local vector and remote for the parents and the children ---*/
      
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        
        Parent_Remote[iVertex] = Buffer_Receive_Parent[iVertex];
        
        /*--- We use the same sorting as in the donor domain ---*/
        
        for (jVertex = 0; jVertex < Aux_Parent.size(); jVertex++) {
          if (Parent_Remote[iVertex] == Aux_Parent[jVertex]) {
            Parent_Local[iVertex] = jVertex + Index_CoarseCV;
            break;
          }
        }
        
        Children_Remote[iVertex] = Buffer_Receive_Children[iVertex];
        Children_Local[iVertex] = fine_grid->vertex[MarkerR][iVertex]->GetNode();
        
      }
      
      Index_CoarseCV += Aux_Parent.size();
      
      nChildren_MPI = new unsigned short [Index_CoarseCV];
      for (iParent = 0; iParent < Index_CoarseCV; iParent++)
        nChildren_MPI[iParent] = 0;
      
      /*--- Create the final structure ---*/
      for (iVertex = 0; iVertex < nVertexR; iVertex++) {
        
        /*--- Be careful, it is possible that a node change the agglomeration configuration, the priority
         is always, when receive the information ---*/
        
        fine_grid->node[Children_Local[iVertex]]->SetParent_CV(Parent_Local[iVertex]);
        node[Parent_Local[iVertex]]->SetChildren_CV(nChildren_MPI[Parent_Local[iVertex]], Children_Local[iVertex]);
        nChildren_MPI[Parent_Local[iVertex]]++;
        node[Parent_Local[iVertex]]->SetnChildren_CV(nChildren_MPI[Parent_Local[iVertex]]);
        node[Parent_Local[iVertex]]->SetDomain(false);
        
      }
      
      /*--- Deallocate auxiliar structures ---*/
      
      delete[] nChildren_MPI;
      delete[] Parent_Remote;
      delete[] Children_Remote;
      delete[] Parent_Local;
      delete[] Children_Local;
      
      /*--- Deallocate receive buffer ---*/
      
      delete [] Buffer_Receive_Children;
      delete [] Buffer_Receive_Parent;
      
    }
    
  }
  
  /*--- Update the number of points after the MPI agglomeration ---*/
  
  nPoint = Index_CoarseCV;
  
  /*--- Console output with the summary of the agglomeration ---*/
  
  Local_nPointCoarse = nPoint;
  Local_nPointFine = fine_grid->GetnPoint();
  
#ifdef HAVE_MPI
  SU2_MPI::Allreduce(&Local_nPointCoarse, &Global_nPointCoarse, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
  SU2_MPI::Allreduce(&Local_nPointFine, &Global_nPointFine, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
  Global_nPointCoarse = Local_nPointCoarse;
  Global_nPointFine = Local_nPointFine;
#endif
  
  su2double Coeff = 1.0, CFL = 0.0, factor = 1.5;
  
  if (iMesh != MESH_0) {
    if (nDim == 2) Coeff = pow(su2double(Global_nPointFine)/su2double(Global_nPointCoarse), 1./2.);
    if (nDim == 3) Coeff = pow(su2double(Global_nPointFine)/su2double(Global_nPointCoarse), 1./3.);
    CFL = factor*config->GetCFL(iMesh-1)/Coeff;
    config->SetCFL(iMesh, CFL);
  }
  
  su2double ratio = su2double(Global_nPointFine)/su2double(Global_nPointCoarse);
  
  if (((nDim == 2) && (ratio < 2.5)) ||
      ((nDim == 3) && (ratio < 2.5))) {
    config->SetMGLevels(iMesh-1);
  }
  else {
    if (rank == MASTER_NODE) {
      if (iMesh == 1) cout <<"MG level: "<< iMesh-1 <<" -> CVs: " << Global_nPointFine << ". Agglomeration rate 1/1.00. CFL "<< config->GetCFL(iMesh-1) <<"." << endl;
      cout <<"MG level: "<< iMesh <<" -> CVs: " << Global_nPointCoarse << ". Agglomeration rate 1/" << ratio <<". CFL "<< CFL <<"." << endl;
    }
  }
 
  delete [] copy_marker;
  
}


CMultiGridGeometry::~CMultiGridGeometry(void) {
  
}

bool CMultiGridGeometry::SetBoundAgglomeration(unsigned long CVPoint, short marker_seed, CGeometry *fine_grid, CConfig *config) {
  
  bool agglomerate_CV = false;
  unsigned short counter, jMarker;
  
  unsigned short nMarker_Max = config->GetnMarker_Max();
  
  unsigned short *copy_marker = new unsigned short [nMarker_Max];

  /*--- Basic condition, the element has not being previously agglomerated, it belongs to the domain,
   and has passed some basic geometrical check ---*/
  
  if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
      (fine_grid->node[CVPoint]->GetDomain()) &&
      (GeometricalCheck(CVPoint, fine_grid, config))) {
    
    /*--- If the element belong to the boundary, we must be careful ---*/
    
    if (fine_grid->node[CVPoint]->GetBoundary()) {
      
      /*--- Identify the markers of the vertex that we want to agglomerate ---*/
      
      counter = 0;
      for (jMarker = 0; jMarker < fine_grid->GetnMarker(); jMarker ++)
        if (fine_grid->node[CVPoint]->GetVertex(jMarker) != -1) {
          copy_marker[counter] = jMarker;
          counter++;
        }
      
      /*--- The basic condition is that the aglomerated vertex must have the same physical marker,
       but eventually a send-receive condition ---*/
      
      /*--- Only one marker in the vertex that is going to be aglomerated ---*/
      
      if (counter == 1) {
        
        /*--- We agglomerate if there is only a marker and is the same marker as the seed marker ---*/
        
        if (copy_marker[0] == marker_seed)
          agglomerate_CV = true;
        
        /*--- If there is only a marker, but the marker is the SEND_RECEIVE ---*/
        
        if (config->GetMarker_All_KindBC(copy_marker[0]) == SEND_RECEIVE)
          agglomerate_CV = true;
        
      }
      
      /*--- If there are two markers in the vertex that is going to be aglomerated ---*/
      
      if (counter == 2) {
        
        /*--- First we verify that the seed is a physical boundary ---*/
        
        if (config->GetMarker_All_KindBC(marker_seed) != SEND_RECEIVE) {
          
          /*--- Then we check that one of the marker is equal to the seed marker, and the other is send/receive ---*/
          
          if (((copy_marker[0] == marker_seed) && (config->GetMarker_All_KindBC(copy_marker[1]) == SEND_RECEIVE)) ||
              ((config->GetMarker_All_KindBC(copy_marker[0]) == SEND_RECEIVE) && (copy_marker[1] == marker_seed)))
            agglomerate_CV = true;
        }
        
      }
      
    }
    
    /*--- If the element belong to the domain, it is allways aglomerated ---*/
    
    else { agglomerate_CV = true; }
    
  }
  
  delete [] copy_marker;

  return agglomerate_CV;

}


bool CMultiGridGeometry::GeometricalCheck(unsigned long iPoint, CGeometry *fine_grid, CConfig *config) {
  
  su2double max_dimension = 1.2;
  
  /*--- Evaluate the total size of the element ---*/
  
  bool Volume = true;
  su2double ratio = pow(fine_grid->node[iPoint]->GetVolume(), 1.0/su2double(nDim))*max_dimension;
  su2double limit = pow(config->GetDomainVolume(), 1.0/su2double(nDim));
  if ( ratio > limit ) Volume = false;
  
  /*--- Evaluate the stretching of the element ---*/
  
  bool Stretching = true;
  
  /*	unsigned short iNode, iDim;
   unsigned long jPoint;
   su2double *Coord_i = fine_grid->node[iPoint]->GetCoord();
   su2double max_dist = 0.0 ; su2double min_dist = 1E20;
   for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
   jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
   su2double *Coord_j = fine_grid->node[jPoint]->GetCoord();
   su2double distance = 0.0;
   for (iDim = 0; iDim < nDim; iDim++)
   distance += (Coord_j[iDim]-Coord_i[iDim])*(Coord_j[iDim]-Coord_i[iDim]);
   distance = sqrt(distance);
   max_dist = max(distance, max_dist);
   min_dist = min(distance, min_dist);
   }
   if ( max_dist/min_dist > 100.0 ) Stretching = false;*/
  
  return (Stretching && Volume);
  
}

void CMultiGridGeometry::SetSuitableNeighbors(vector<unsigned long> *Suitable_Indirect_Neighbors, unsigned long iPoint,
                                              unsigned long Index_CoarseCV, CGeometry *fine_grid) {
  
  unsigned long jPoint, kPoint, lPoint;
  unsigned short iNode, jNode, iNeighbor, jNeighbor, kNode;
  bool SecondNeighborSeed, ThirdNeighborSeed;
  vector<unsigned long>::iterator it;
  
  /*--- Create a list with the first neighbors, including the seed ---*/
  
  vector<unsigned long> First_Neighbor_Points;
  First_Neighbor_Points.push_back(iPoint);
  for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
    jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
    First_Neighbor_Points.push_back(jPoint);
  }
  
  /*--- Create a list with the second neighbors, without first, and seed neighbors ---*/
  
  vector<unsigned long> Second_Neighbor_Points, Second_Origin_Points, Suitable_Second_Neighbors;
  
  for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
    jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
    
    for (jNode = 0; jNode <	fine_grid->node[jPoint]->GetnPoint(); jNode ++) {
      kPoint = fine_grid->node[jPoint]->GetPoint(jNode);
      
      /*--- Check that the second neighbor do not belong to the first neighbor or the seed ---*/
      
      SecondNeighborSeed = true;
      for (iNeighbor = 0; iNeighbor <	First_Neighbor_Points.size(); iNeighbor ++)
        if (kPoint == First_Neighbor_Points[iNeighbor]) {
          SecondNeighborSeed = false; break;
        }
      
      if (SecondNeighborSeed) {
        Second_Neighbor_Points.push_back(kPoint);
        Second_Origin_Points.push_back(jPoint);
      }
      
    }
  }
  
  /*---  Identify those second neighbors that are repeated (candidate to be added) ---*/
  
  for (iNeighbor = 0; iNeighbor <	Second_Neighbor_Points.size(); iNeighbor ++)
    
    for (jNeighbor = 0; jNeighbor <	Second_Neighbor_Points.size(); jNeighbor ++)
      
    /*--- Repeated second neighbor with different origin ---*/
      
      if ((Second_Neighbor_Points[iNeighbor] == Second_Neighbor_Points[jNeighbor]) &&
          (Second_Origin_Points[iNeighbor] != Second_Origin_Points[jNeighbor]) &&
          (iNeighbor < jNeighbor)) {
        
        Suitable_Indirect_Neighbors->push_back(Second_Neighbor_Points[iNeighbor]);
        
        /*--- Create alist with the suitable second neighbor, that we will use
         to compute the third neighbors --*/
        
        Suitable_Second_Neighbors.push_back(Second_Neighbor_Points[iNeighbor]);
        
      }
  
  
  /*--- Remove repeated from the suitable second neighbors ---*/
  
  sort(Suitable_Second_Neighbors.begin(), Suitable_Second_Neighbors.end());
  it = unique(Suitable_Second_Neighbors.begin(), Suitable_Second_Neighbors.end());
  Suitable_Second_Neighbors.resize(it - Suitable_Second_Neighbors.begin());
  
  /*--- Remove repeated from first neighbors ---*/
  
  sort(First_Neighbor_Points.begin(), First_Neighbor_Points.end());
  it = unique(First_Neighbor_Points.begin(), First_Neighbor_Points.end());
  First_Neighbor_Points.resize(it - First_Neighbor_Points.begin());
  
  /*--- Create a list with the third neighbors, without first, second, and seed neighbors ---*/
  
  vector<unsigned long> Third_Neighbor_Points, Third_Origin_Points;
  
  for (jNode = 0; jNode <	Suitable_Second_Neighbors.size(); jNode ++) {
    kPoint = Suitable_Second_Neighbors[jNode];
    
    for (kNode = 0; kNode <	fine_grid->node[kPoint]->GetnPoint(); kNode ++) {
      lPoint = fine_grid->node[kPoint]->GetPoint(kNode);
      
      /*--- Check that the third neighbor do not belong to the first neighbors or the seed ---*/
      
      ThirdNeighborSeed = true;
      
      for (iNeighbor = 0; iNeighbor <	First_Neighbor_Points.size(); iNeighbor ++)
        if (lPoint == First_Neighbor_Points[iNeighbor]) {
          ThirdNeighborSeed = false;
          break;
        }
      
      /*--- Check that the third neighbor do not belong to the second neighbors ---*/
      
      for (iNeighbor = 0; iNeighbor <	Suitable_Second_Neighbors.size(); iNeighbor ++)
        if (lPoint == Suitable_Second_Neighbors[iNeighbor]) {
          ThirdNeighborSeed = false;
          break;
        }
      
      if (ThirdNeighborSeed) {
        Third_Neighbor_Points.push_back(lPoint);
        Third_Origin_Points.push_back(kPoint);
      }
      
    }
  }
  
  /*---  Identify those third neighbors that are repeated (candidate to be added) ---*/
  
  for (iNeighbor = 0; iNeighbor <	Third_Neighbor_Points.size(); iNeighbor ++)
    for (jNeighbor = 0; jNeighbor <	Third_Neighbor_Points.size(); jNeighbor ++)
      
    /*--- Repeated second neighbor with different origin ---*/
      
      if ((Third_Neighbor_Points[iNeighbor] == Third_Neighbor_Points[jNeighbor]) &&
          (Third_Origin_Points[iNeighbor] != Third_Origin_Points[jNeighbor]) &&
          (iNeighbor < jNeighbor)) {
        
        Suitable_Indirect_Neighbors->push_back(Third_Neighbor_Points[iNeighbor]);
        
      }
  
  /*--- Remove repeated from Suitable Indirect Neighbors List ---*/
  
  sort(Suitable_Indirect_Neighbors->begin(), Suitable_Indirect_Neighbors->end());
  it = unique(Suitable_Indirect_Neighbors->begin(), Suitable_Indirect_Neighbors->end());
  Suitable_Indirect_Neighbors->resize(it - Suitable_Indirect_Neighbors->begin());
  
}

void CMultiGridGeometry::SetPoint_Connectivity(CGeometry *fine_grid) {
  
  unsigned long iFinePoint, iFinePoint_Neighbor, iParent, iCoarsePoint;
  unsigned short iChildren, iNode;
  
  /*--- Set the point surrounding a point ---*/
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++) {
    for (iChildren = 0; iChildren <  node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
      iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
      for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode ++) {
        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
        if (iParent != iCoarsePoint) node[iCoarsePoint]->SetPoint(iParent);
      }
    }
  }
  
  /*--- Set the number of neighbors variable, this is
   important for JST and multigrid in parallel ---*/
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++)
    node[iCoarsePoint]->SetnNeighbor(node[iCoarsePoint]->GetnPoint());
  
}

void CMultiGridGeometry::SetVertex(CGeometry *fine_grid, CConfig *config) {
  unsigned long  iVertex, iFinePoint, iCoarsePoint;
  unsigned short iMarker, iMarker_Tag, iChildren;
  
  nMarker = fine_grid->GetnMarker();
  unsigned short nMarker_Max = config->GetnMarker_Max();

  /*--- If any children node belong to the boundary then the entire control
   volume will belong to the boundary ---*/
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++)
    for (iChildren = 0; iChildren <	node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
      iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
      if (fine_grid->node[iFinePoint]->GetBoundary()) {
        node[iCoarsePoint]->SetBoundary(nMarker);
        break;
      }
    }
  
  vertex = new CVertex**[nMarker];
  nVertex = new unsigned long [nMarker];
  
  Tag_to_Marker = new string [nMarker_Max];
  for (iMarker_Tag = 0; iMarker_Tag < nMarker_Max; iMarker_Tag++)
    Tag_to_Marker[iMarker_Tag] = fine_grid->GetMarker_Tag(iMarker_Tag);
  
  /*--- Compute the number of vertices to do the dimensionalization ---*/
  for (iMarker = 0; iMarker < nMarker; iMarker++) nVertex[iMarker] = 0;
  
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++) {
    if (node[iCoarsePoint]->GetBoundary()) {
      for (iChildren = 0; iChildren <	node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
        for (iMarker = 0; iMarker < nMarker; iMarker ++) {
          if ((fine_grid->node[iFinePoint]->GetVertex(iMarker) != -1) && (node[iCoarsePoint]->GetVertex(iMarker) == -1)) {
            iVertex = nVertex[iMarker];
            node[iCoarsePoint]->SetVertex(iVertex, iMarker);
            nVertex[iMarker]++;
          }
        }
      }
    }
  }
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    vertex[iMarker] = new CVertex* [fine_grid->GetnVertex(iMarker)+1];
    nVertex[iMarker] = 0;
  }
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++)
    if (node[iCoarsePoint]->GetBoundary())
      for (iMarker = 0; iMarker < nMarker; iMarker ++)
        node[iCoarsePoint]->SetVertex(-1, iMarker);
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) nVertex[iMarker] = 0;
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++)
    if (node[iCoarsePoint]->GetBoundary())
      for (iChildren = 0; iChildren <	node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
        for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker ++) {
          if ((fine_grid->node[iFinePoint]->GetVertex(iMarker) != -1) && (node[iCoarsePoint]->GetVertex(iMarker) == -1)) {
            iVertex = nVertex[iMarker];
            vertex[iMarker][iVertex] = new CVertex(iCoarsePoint, nDim);
            node[iCoarsePoint]->SetVertex(iVertex, iMarker);
            
            /*--- Set the transformation to apply ---*/
            unsigned long ChildVertex = fine_grid->node[iFinePoint]->GetVertex(iMarker);
            unsigned short RotationKind = fine_grid->vertex[iMarker][ChildVertex]->GetRotation_Type();
            vertex[iMarker][iVertex]->SetRotation_Type(RotationKind);
            nVertex[iMarker]++;
          }
        }
      }
}

void CMultiGridGeometry::MatchNearField(CConfig *config) {
  
  unsigned short iMarker;
  unsigned long iVertex, iPoint;
  int iProcessor;
  
#ifndef HAVE_MPI
  iProcessor = MASTER_NODE;
#else
  MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == NEARFIELD_BOUNDARY) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          vertex[iMarker][iVertex]->SetDonorPoint(iPoint, node[iPoint]->GetGlobalIndex(), iVertex, iMarker, iProcessor);
        }
      }
    }
  }
  
}

void CMultiGridGeometry::MatchActuator_Disk(CConfig *config) {
  
  unsigned short iMarker;
  unsigned long iVertex, iPoint;
  int iProcessor;
  
#ifndef HAVE_MPI
  iProcessor = MASTER_NODE;
#else
  MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if ((config->GetMarker_All_KindBC(iMarker) == ACTDISK_INLET) ||
        (config->GetMarker_All_KindBC(iMarker) == ACTDISK_OUTLET)) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          vertex[iMarker][iVertex]->SetDonorPoint(iPoint, node[iPoint]->GetGlobalIndex(), iVertex, iMarker, iProcessor);
        }
      }
    }
  }
  
}

void CMultiGridGeometry::MatchInterface(CConfig *config) {
  
  unsigned short iMarker;
  unsigned long iVertex, iPoint;
  int iProcessor;
  
#ifndef HAVE_MPI
  iProcessor = MASTER_NODE;
#else
  MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == INTERFACE_BOUNDARY) {
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        if (node[iPoint]->GetDomain()) {
          vertex[iMarker][iVertex]->SetDonorPoint(iPoint, node[iPoint]->GetGlobalIndex(), iVertex, iMarker, iProcessor);
        }
      }
    }
  }
  
}

void CMultiGridGeometry::SetControlVolume(CConfig *config, CGeometry *fine_grid, unsigned short action) {
  
  unsigned long iFinePoint, iFinePoint_Neighbor, iCoarsePoint, iEdge, iParent;
  long FineEdge, CoarseEdge;
  unsigned short iChildren, iNode, iDim;
  bool change_face_orientation;
  su2double *Normal, Coarse_Volume, Area, *NormalFace = NULL;
  Normal = new su2double [nDim];
  
  /*--- Compute the area of the coarse volume ---*/
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++) {
    node[iCoarsePoint]->SetVolume(0.0);
    Coarse_Volume = 0.0;
    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
      iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
      Coarse_Volume += fine_grid->node[iFinePoint]->GetVolume();
    }
    node[iCoarsePoint]->SetVolume(Coarse_Volume);
  }
  
  /*--- Update or not the values of faces at the edge ---*/
  if (action != ALLOCATE) {
    for (iEdge=0; iEdge < nEdge; iEdge++)
      edge[iEdge]->SetZeroValues();
  }
  
  for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint ++)
    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
      iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
      
      for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode ++) {
        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
        if ((iParent != iCoarsePoint) && (iParent < iCoarsePoint)) {
          
          FineEdge = fine_grid->FindEdge(iFinePoint, iFinePoint_Neighbor);
          
          change_face_orientation = false;
          if (iFinePoint < iFinePoint_Neighbor) change_face_orientation = true;
          
          CoarseEdge = FindEdge(iParent, iCoarsePoint);
          
          fine_grid->edge[FineEdge]->GetNormal(Normal);
          
          if (change_face_orientation) {
            for (iDim = 0; iDim < nDim; iDim++) Normal[iDim] = -Normal[iDim];
            edge[CoarseEdge]->AddNormal(Normal);
          }
          else {
            edge[CoarseEdge]->AddNormal(Normal);
          }
        }
      }
    }
  delete[] Normal;
  
  /*--- Check if there is a normal with null area ---*/
  
  for (iEdge = 0; iEdge < nEdge; iEdge++) {
    NormalFace = edge[iEdge]->GetNormal();
    Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim]*NormalFace[iDim];
    Area = sqrt(Area);
    if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = EPS*EPS;
  }
  
}

void CMultiGridGeometry::SetBoundControlVolume(CConfig *config, CGeometry *fine_grid, unsigned short action) {
  unsigned long iCoarsePoint, iFinePoint, FineVertex, iVertex;
  unsigned short iMarker, iChildren, iDim;
  su2double *Normal, Area, *NormalFace = NULL;
  
  Normal = new su2double [nDim];
  
  if (action != ALLOCATE) {
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
        vertex[iMarker][iVertex]->SetZeroValues();
  }
  
  for (iMarker = 0; iMarker < nMarker; iMarker ++)
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      iCoarsePoint = vertex[iMarker][iVertex]->GetNode();
      for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren ++) {
        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
        if (fine_grid->node[iFinePoint]->GetVertex(iMarker)!=-1) {
          FineVertex = fine_grid->node[iFinePoint]->GetVertex(iMarker);
          fine_grid->vertex[iMarker][FineVertex]->GetNormal(Normal);
          vertex[iMarker][iVertex]->AddNormal(Normal);
        }
      }
    }
  
  delete[] Normal;
  
  /*--- Check if there is a normal with null area ---*/
  for (iMarker = 0; iMarker < nMarker; iMarker ++)
    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
      NormalFace = vertex[iMarker][iVertex]->GetNormal();
      Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim]*NormalFace[iDim];
      Area = sqrt(Area);
      if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = EPS*EPS;
    }
  
}

void CMultiGridGeometry::SetCoord(CGeometry *geometry) {
  unsigned long Point_Fine, Point_Coarse;
  unsigned short iChildren, iDim;
  su2double Area_Parent, Area_Children;
  su2double *Coordinates_Fine, *Coordinates;
  Coordinates = new su2double[nDim];
  
  for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++) {
    Area_Parent = node[Point_Coarse]->GetVolume();
    for (iDim = 0; iDim < nDim; iDim++) Coordinates[iDim] = 0.0;
    for (iChildren = 0; iChildren < node[Point_Coarse]->GetnChildren_CV(); iChildren++) {
      Point_Fine = node[Point_Coarse]->GetChildren_CV(iChildren);
      Area_Children = geometry->node[Point_Fine]->GetVolume();
      Coordinates_Fine = geometry->node[Point_Fine]->GetCoord();
      for (iDim = 0; iDim < nDim; iDim++)
        Coordinates[iDim] += Coordinates_Fine[iDim]*Area_Children/Area_Parent;
    }
    for (iDim = 0; iDim < nDim; iDim++)
      node[Point_Coarse]->SetCoord(iDim, Coordinates[iDim]);
  }
  delete[] Coordinates;
}

void CMultiGridGeometry::SetRotationalVelocity(CConfig *config, unsigned short val_iZone, bool print) {
  
  unsigned long iPoint_Coarse;
  su2double *RotVel, Distance[3] = {0.0,0.0,0.0}, *Coord;
  su2double Center[3] = {0.0,0.0,0.0}, Omega[3] = {0.0,0.0,0.0}, L_Ref;
  RotVel = new su2double [3];
  
  /*--- Center of rotation & angular velocity vector from config. ---*/
  
  Center[0] = config->GetMotion_Origin_X(val_iZone);
  Center[1] = config->GetMotion_Origin_Y(val_iZone);
  Center[2] = config->GetMotion_Origin_Z(val_iZone);
  Omega[0]  = config->GetRotation_Rate_X(val_iZone)/config->GetOmega_Ref();
  Omega[1]  = config->GetRotation_Rate_Y(val_iZone)/config->GetOmega_Ref();
  Omega[2]  = config->GetRotation_Rate_Z(val_iZone)/config->GetOmega_Ref();
  L_Ref     = config->GetLength_Ref();
  
  /*--- Loop over all nodes and set the rotational velocity. ---*/
  
  for (iPoint_Coarse = 0; iPoint_Coarse < GetnPoint(); iPoint_Coarse++) {
    
    /*--- Get the coordinates of the current node ---*/
    
    Coord = node[iPoint_Coarse]->GetCoord();
    
    /*--- Calculate the non-dim. distance from the rotation center ---*/
    
    Distance[0] = (Coord[0]-Center[0])/L_Ref;
    Distance[1] = (Coord[1]-Center[1])/L_Ref;
    Distance[2] = (Coord[2]-Center[2])/L_Ref;
    
    /*--- Calculate the angular velocity as omega X r ---*/
    
    RotVel[0] = Omega[1]*(Distance[2]) - Omega[2]*(Distance[1]);
    RotVel[1] = Omega[2]*(Distance[0]) - Omega[0]*(Distance[2]);
    RotVel[2] = Omega[0]*(Distance[1]) - Omega[1]*(Distance[0]);
    
    /*--- Store the grid velocity at this node ---*/
    
    node[iPoint_Coarse]->SetGridVel(RotVel);
    
  }
  
  delete [] RotVel;
  
}

void CMultiGridGeometry::SetShroudVelocity(CConfig *config) {

  unsigned long iPoint, iVertex;
  unsigned short iMarker, iMarkerShroud;
  su2double RotVel[3];

  RotVel[0] = 0.0;
  RotVel[1] = 0.0;
  RotVel[2] = 0.0;

  /*--- Loop over all vertex in the shroud marker and set the rotational velocity to 0.0 ---*/
  for (iMarker = 0; iMarker < nMarker; iMarker++){
    for(iMarkerShroud=0; iMarkerShroud < config->GetnMarker_Shroud(); iMarkerShroud++){
      if(config->GetMarker_Shroud(iMarkerShroud) == config->GetMarker_All_TagBound(iMarker)){
        for (iVertex = 0; iVertex  < nVertex[iMarker]; iVertex++) {
          iPoint = vertex[iMarker][iVertex]->GetNode();
          node[iPoint]->SetGridVel(RotVel);
        }
      }
    }
  }
}

void CMultiGridGeometry::SetTranslationalVelocity(CConfig *config, unsigned short val_iZone, bool print) {
  
  unsigned iDim;
  unsigned long iPoint_Coarse;
  su2double xDot[3] = {0.0,0.0,0.0};
  
  /*--- Get the translational velocity vector from config ---*/
  
  xDot[0]   = config->GetTranslation_Rate_X(val_iZone)/config->GetVelocity_Ref();
  xDot[1]   = config->GetTranslation_Rate_Y(val_iZone)/config->GetVelocity_Ref();
  xDot[2]   = config->GetTranslation_Rate_Z(val_iZone)/config->GetVelocity_Ref();
  
  /*--- Loop over all nodes and set the translational velocity ---*/
  
  for (iPoint_Coarse = 0; iPoint_Coarse < nPoint; iPoint_Coarse++) {
    
    /*--- Store the grid velocity at this node ---*/
    
    for (iDim = 0; iDim < nDim; iDim++)
      node[iPoint_Coarse]->SetGridVel(iDim,xDot[iDim]);
    
  }
  
}

void CMultiGridGeometry::SetGridVelocity(CConfig *config, unsigned long iter) {
  
  /*--- Local variables ---*/
  
  su2double *Coord_nP1 = NULL, *Coord_n = NULL, *Coord_nM1 = NULL;
  su2double TimeStep, GridVel = 0.0;
  unsigned long Point_Coarse;
  unsigned short iDim;
  
  /*--- Compute the velocity of each node in the volume mesh ---*/
  
  for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++) {
    
    /*--- Coordinates of the current point at n+1, n, & n-1 time levels ---*/
    
    Coord_nM1 = node[Point_Coarse]->GetCoord_n1();
    Coord_n   = node[Point_Coarse]->GetCoord_n();
    Coord_nP1 = node[Point_Coarse]->GetCoord();
    
    /*--- Unsteady time step ---*/
    
    TimeStep = config->GetDelta_UnstTimeND();
    
    /*--- Compute mesh velocity with 1st or 2nd-order approximation ---*/
    
    for (iDim = 0; iDim < nDim; iDim++) {
      if (config->GetUnsteady_Simulation() == DT_STEPPING_1ST)
        GridVel = ( Coord_nP1[iDim] - Coord_n[iDim] ) / TimeStep;
      if (config->GetUnsteady_Simulation() == DT_STEPPING_2ND)
        GridVel = ( 3.0*Coord_nP1[iDim] - 4.0*Coord_n[iDim]
                   +  1.0*Coord_nM1[iDim] ) / (2.0*TimeStep);
      
      /*--- Store grid velocity for this point ---*/
      
      node[Point_Coarse]->SetGridVel(iDim, GridVel);
      
    }
  }
}

void CMultiGridGeometry::SetRestricted_GridVelocity(CGeometry *fine_mesh, CConfig *config) {
  
  /*--- Local variables ---*/
  unsigned short iDim, iChild;
  unsigned long Point_Coarse, Point_Fine;
  su2double Area_Parent, Area_Child, Grid_Vel[3], *Grid_Vel_Fine;
  
  /*--- Loop over all coarse mesh points ---*/
  for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++) {
    Area_Parent = node[Point_Coarse]->GetVolume();
    
    /*--- Zero out the grid velocity ---*/
    for (iDim = 0; iDim < nDim; iDim++)
      Grid_Vel[iDim] = 0.0;
    
    /*--- Loop over all of the children for this coarse CV and compute
     a grid velocity based on the values in the child CVs (fine mesh). ---*/
    for (iChild = 0; iChild < node[Point_Coarse]->GetnChildren_CV(); iChild++) {
      Point_Fine    = node[Point_Coarse]->GetChildren_CV(iChild);
      Area_Child    = fine_mesh->node[Point_Fine]->GetVolume();
      Grid_Vel_Fine = fine_mesh->node[Point_Fine]->GetGridVel();
      for (iDim = 0; iDim < nDim; iDim++)
        Grid_Vel[iDim] += Grid_Vel_Fine[iDim]*Area_Child/Area_Parent;
    }
    
    /*--- Set the grid velocity for this coarse node. ---*/
    for (iDim = 0; iDim < nDim; iDim++)
      node[Point_Coarse]->SetGridVel(iDim, Grid_Vel[iDim]);
  }
}


void CMultiGridGeometry::FindNormal_Neighbor(CConfig *config) {
  
  unsigned short iMarker, iDim;
  unsigned long iPoint, iVertex;
  
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    
    if (config->GetMarker_All_KindBC(iMarker) != SEND_RECEIVE &&
        config->GetMarker_All_KindBC(iMarker) != INTERFACE_BOUNDARY &&
        config->GetMarker_All_KindBC(iMarker) != NEARFIELD_BOUNDARY ) {
      
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        
        iPoint = vertex[iMarker][iVertex]->GetNode();
        
        /*--- If the node belong to the domain ---*/
        if (node[iPoint]->GetDomain()) {
          
          /*--- Compute closest normal neighbor ---*/
          su2double cos_max, scalar_prod, norm_vect, norm_Normal, cos_alpha, diff_coord;
          unsigned long Point_Normal = 0, jPoint;
          unsigned short iNeigh;
          su2double *Normal = vertex[iMarker][iVertex]->GetNormal();
          cos_max = -1.0;
          for (iNeigh = 0; iNeigh < node[iPoint]->GetnPoint(); iNeigh++) {
            jPoint = node[iPoint]->GetPoint(iNeigh);
            scalar_prod = 0.0; norm_vect = 0.0; norm_Normal = 0.0;
            for (iDim = 0; iDim < nDim; iDim++) {
              diff_coord = node[jPoint]->GetCoord(iDim)-node[iPoint]->GetCoord(iDim);
              scalar_prod += diff_coord*Normal[iDim];
              norm_vect += diff_coord*diff_coord;
              norm_Normal += Normal[iDim]*Normal[iDim];
            }
            norm_vect = sqrt(norm_vect);
            norm_Normal = sqrt(norm_Normal);
            cos_alpha = scalar_prod/(norm_vect*norm_Normal);
            
            /*--- Get maximum cosine (not minimum because normals are oriented inwards) ---*/
            if (cos_alpha >= cos_max) {
              Point_Normal = jPoint;
              cos_max = cos_alpha;
            }
          }
          vertex[iMarker][iVertex]->SetNormal_Neighbor(Point_Normal);
        }
      }
    }
  }
}


void CMultiGridGeometry::SetGeometryPlanes(CConfig *config) {
  bool loop_on;
  unsigned short iMarker = 0;
  su2double auxXCoord, auxYCoord, auxZCoord,	*Face_Normal = NULL, auxArea, *Xcoord = NULL, *Ycoord = NULL, *Zcoord = NULL, *FaceArea = NULL;
  unsigned long jVertex, iVertex, ixCoord, iPoint, iVertex_Wall, nVertex_Wall = 0;
  
  /*--- Compute the total number of points on the near-field ---*/
  nVertex_Wall = 0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if ((config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)               ||
        (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)              ||
        (config->GetMarker_All_KindBC(iMarker) == EULER_WALL)                )
      nVertex_Wall += nVertex[iMarker];
  
  
  /*--- Create an array with all the coordinates, points, pressures, face area,
   equivalent area, and nearfield weight ---*/
  Xcoord = new su2double[nVertex_Wall];
  Ycoord = new su2double[nVertex_Wall];
  if (nDim == 3)	Zcoord = new su2double[nVertex_Wall];
  FaceArea = new su2double[nVertex_Wall];
  
  /*--- Copy the boundary information to an array ---*/
  iVertex_Wall = 0;
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if ((config->GetMarker_All_KindBC(iMarker) == HEAT_FLUX)               ||
        (config->GetMarker_All_KindBC(iMarker) == ISOTHERMAL)              ||
        (config->GetMarker_All_KindBC(iMarker) == EULER_WALL)                )
      for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++) {
        iPoint = vertex[iMarker][iVertex]->GetNode();
        Xcoord[iVertex_Wall] = node[iPoint]->GetCoord(0);
        Ycoord[iVertex_Wall] = node[iPoint]->GetCoord(1);
        if (nDim==3) Zcoord[iVertex_Wall] = node[iPoint]->GetCoord(2);
        Face_Normal = vertex[iMarker][iVertex]->GetNormal();
        FaceArea[iVertex_Wall] = fabs(Face_Normal[nDim-1]);
        iVertex_Wall ++;
      }
  
  
  //vector<su2double> XCoordList;
  vector<su2double>::iterator IterXCoordList;
  
  for (iVertex = 0; iVertex < nVertex_Wall; iVertex++)
    XCoordList.push_back(Xcoord[iVertex]);
  
  sort( XCoordList.begin(), XCoordList.end());
  IterXCoordList = unique( XCoordList.begin(), XCoordList.end());
  XCoordList.resize( IterXCoordList - XCoordList.begin() );
  
  /*--- Create vectors and distribute the values among the different PhiAngle queues ---*/
  Xcoord_plane.resize(XCoordList.size());
  Ycoord_plane.resize(XCoordList.size());
  if (nDim==3) Zcoord_plane.resize(XCoordList.size());
  FaceArea_plane.resize(XCoordList.size());
  Plane_points.resize(XCoordList.size());
  
  
  su2double dist_ratio;
  unsigned long iCoord;
  
  /*--- Distribute the values among the different PhiAngles ---*/
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    if (node[iPoint]->GetDomain()) {
      loop_on = true;
      for (ixCoord = 0; ixCoord < XCoordList.size()-1 && loop_on; ixCoord++) {
        dist_ratio = (node[iPoint]->GetCoord(0) - XCoordList[ixCoord])/(XCoordList[ixCoord+1]- XCoordList[ixCoord]);
        if (dist_ratio >= 0 && dist_ratio <= 1.0) {
          if (dist_ratio <= 0.5) iCoord = ixCoord;
          else iCoord = ixCoord+1;
          Xcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(0) );
          Ycoord_plane[iCoord].push_back(node[iPoint]->GetCoord(1) );
          if (nDim==3) Zcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(2) );
          FaceArea_plane[iCoord].push_back(node[iPoint]->GetVolume());   ///// CHECK AREA CALCULATION
          Plane_points[iCoord].push_back(iPoint );
          loop_on = false;
        }
      }
    }
  }
  
  unsigned long auxPoint;
  /*--- Order the arrays in ascending values of y ---*/
  for (ixCoord = 0; ixCoord < XCoordList.size(); ixCoord++)
    for (iVertex = 0; iVertex < Xcoord_plane[ixCoord].size(); iVertex++)
      for (jVertex = 0; jVertex < Xcoord_plane[ixCoord].size() - 1 - iVertex; jVertex++)
        if (Ycoord_plane[ixCoord][jVertex] > Ycoord_plane[ixCoord][jVertex+1]) {
          auxXCoord = Xcoord_plane[ixCoord][jVertex]; Xcoord_plane[ixCoord][jVertex] = Xcoord_plane[ixCoord][jVertex+1]; Xcoord_plane[ixCoord][jVertex+1] = auxXCoord;
          auxYCoord = Ycoord_plane[ixCoord][jVertex]; Ycoord_plane[ixCoord][jVertex] = Ycoord_plane[ixCoord][jVertex+1]; Ycoord_plane[ixCoord][jVertex+1] = auxYCoord;
          auxPoint = Plane_points[ixCoord][jVertex]; Plane_points[ixCoord][jVertex] = Plane_points[ixCoord][jVertex+1]; Plane_points[ixCoord][jVertex+1] = auxPoint;
          if (nDim==3) {
            auxZCoord = Zcoord_plane[ixCoord][jVertex]; Zcoord_plane[ixCoord][jVertex] = Zcoord_plane[ixCoord][jVertex+1]; Zcoord_plane[ixCoord][jVertex+1] = auxZCoord;
          }
          auxArea = FaceArea_plane[ixCoord][jVertex]; FaceArea_plane[ixCoord][jVertex] = FaceArea_plane[ixCoord][jVertex+1]; FaceArea_plane[ixCoord][jVertex+1] = auxArea;
        }
  
  /*--- Delete structures ---*/
  delete[] Xcoord; delete[] Ycoord;
  if (Zcoord != NULL) delete[] Zcoord;
  delete[] FaceArea;
}

CPeriodicGeometry::CPeriodicGeometry(CGeometry *geometry, CConfig *config) {
  unsigned long nElem_new, nPoint_new, jPoint, iPoint, iElem, jElem, iVertex,
  nelem_triangle = 0, nelem_quad = 0, nelem_tetra = 0, nelem_hexa = 0, nelem_prism = 0,
  nelem_pyramid = 0, iIndex, newElementsBound = 0;
  unsigned short  iMarker, nPeriodic = 0, iPeriodic;
  su2double *center, *angles, rotMatrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}},
  translation[3], *trans, theta, phi, psi, cosTheta, sinTheta, cosPhi, sinPhi, cosPsi, sinPsi,
  dx, dy, dz, rotCoord[3], *Coord_i;
  unsigned short nMarker_Max = config->GetnMarker_Max();

  /*--- We only create the mirror structure for the second boundary ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY) {
      /*--- Evaluate the number of periodic boundary conditions ---*/
      nPeriodic++;
    }
  }
  bool *CreateMirror = new bool[nPeriodic+1];
  CreateMirror[0] = false;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (iPeriodic <= nPeriodic/2) CreateMirror[iPeriodic] = false;
    else CreateMirror[iPeriodic] = true;
  }
  
  /*--- Write the number of dimensions of the problem ---*/
  nDim = geometry->GetnDim();
  
  /*--- Copy the new boundary element information from the geometry class.
   Be careful, as these are pointers to vectors/objects. ---*/
  nNewElem_BoundPer = geometry->nNewElem_Bound;
  newBoundPer       = geometry->newBound;
  
  /*--- Count the number of new boundary elements. ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    newElementsBound += nNewElem_BoundPer[iMarker];
  
  /*--- Loop over the original grid to perform the dimensionalizaton of the new vectors ---*/
  nElem_new = 0; nPoint_new = 0;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      nElem_new += geometry->PeriodicElem[iPeriodic].size();
      nPoint_new += geometry->PeriodicPoint[iPeriodic][0].size();
    }
  }
  
  cout << "Number of new points: " << nPoint_new << "." << endl;
  cout << "Number of new interior elements: " << nElem_new << "." << endl;
  cout << "Number of new boundary elements added to preexisting markers: " << newElementsBound << "." << endl;
  
  /*--- Create a copy of the original grid ---*/
  elem = new CPrimalGrid*[geometry->GetnElem() + nElem_new];
  for (iElem = 0; iElem < geometry->GetnElem(); iElem ++) {
    switch(geometry->elem[iElem]->GetVTK_Type()) {
      case TRIANGLE:
        elem[iElem] = new CTriangle(geometry->elem[iElem]->GetNode(0),
                                    geometry->elem[iElem]->GetNode(1),
                                    geometry->elem[iElem]->GetNode(2), 2);
        nelem_triangle++;
        break;
        
      case QUADRILATERAL:
        elem[iElem] = new CQuadrilateral(geometry->elem[iElem]->GetNode(0),
                                     geometry->elem[iElem]->GetNode(1),
                                     geometry->elem[iElem]->GetNode(2),
                                     geometry->elem[iElem]->GetNode(3), 2);
        nelem_quad++;
        break;
        
      case TETRAHEDRON:
        elem[iElem] = new CTetrahedron(geometry->elem[iElem]->GetNode(0),
                                       geometry->elem[iElem]->GetNode(1),
                                       geometry->elem[iElem]->GetNode(2),
                                       geometry->elem[iElem]->GetNode(3));
        nelem_tetra++;
        break;
        
      case HEXAHEDRON:
        elem[iElem] = new CHexahedron(geometry->elem[iElem]->GetNode(0),
                                      geometry->elem[iElem]->GetNode(1),
                                      geometry->elem[iElem]->GetNode(2),
                                      geometry->elem[iElem]->GetNode(3),
                                      geometry->elem[iElem]->GetNode(4),
                                      geometry->elem[iElem]->GetNode(5),
                                      geometry->elem[iElem]->GetNode(6),
                                      geometry->elem[iElem]->GetNode(7));
        nelem_hexa++;
        break;
        
      case PRISM:
        elem[iElem] = new CPrism(geometry->elem[iElem]->GetNode(0),
                                 geometry->elem[iElem]->GetNode(1),
                                 geometry->elem[iElem]->GetNode(2),
                                 geometry->elem[iElem]->GetNode(3),
                                 geometry->elem[iElem]->GetNode(4),
                                 geometry->elem[iElem]->GetNode(5));
        nelem_prism++;
        break;
        
      case PYRAMID:
        elem[iElem] = new CPyramid(geometry->elem[iElem]->GetNode(0),
                                   geometry->elem[iElem]->GetNode(1),
                                   geometry->elem[iElem]->GetNode(2),
                                   geometry->elem[iElem]->GetNode(3),
                                   geometry->elem[iElem]->GetNode(4));
        nelem_pyramid++;
        break;
        
    }
  }
  
  /*--- Create a list with all the points and the new index ---*/
  unsigned long *Index = new unsigned long [geometry->GetnPoint()];
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint ++) Index[iPoint] = 0;
  
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      for (iIndex = 0; iIndex < geometry->PeriodicPoint[iPeriodic][0].size(); iIndex++) {
        iPoint =  geometry->PeriodicPoint[iPeriodic][0][iIndex];
        Index[iPoint] = geometry->PeriodicPoint[iPeriodic][1][iIndex];
      }
    }
  }
  
  for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++)
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY)
      for (iVertex = 0; iVertex < geometry->GetnVertex(iMarker); iVertex++) {
        iPoint = geometry->vertex[iMarker][iVertex]->GetNode();
        jPoint = geometry->vertex[iMarker][iVertex]->GetDonorPoint();
        Index[iPoint] = jPoint;
      }
  
  /*--- Add the new elements due to the periodic boundary condtion ---*/
  iElem = geometry->GetnElem();
  
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      for (iIndex = 0; iIndex < geometry->PeriodicElem[iPeriodic].size(); iIndex++) {
        jElem = geometry->PeriodicElem[iPeriodic][iIndex];
        
        switch(geometry->elem[jElem]->GetVTK_Type()) {
          case TRIANGLE:
            elem[iElem] = new CTriangle(Index[geometry->elem[jElem]->GetNode(0)],
                                        Index[geometry->elem[jElem]->GetNode(1)],
                                        Index[geometry->elem[jElem]->GetNode(2)], 2);
            iElem++; nelem_triangle++;
            break;
            
          case QUADRILATERAL:
            elem[iElem] = new CQuadrilateral(Index[geometry->elem[jElem]->GetNode(0)],
                                         Index[geometry->elem[jElem]->GetNode(1)],
                                         Index[geometry->elem[jElem]->GetNode(2)],
                                         Index[geometry->elem[jElem]->GetNode(3)], 2);
            iElem++; nelem_quad++;
            break;
            
          case TETRAHEDRON:
            elem[iElem] = new CTetrahedron(Index[geometry->elem[jElem]->GetNode(0)],
                                           Index[geometry->elem[jElem]->GetNode(1)],
                                           Index[geometry->elem[jElem]->GetNode(2)],
                                           Index[geometry->elem[jElem]->GetNode(3)]);
            iElem++; nelem_tetra++;
            break;
            
          case HEXAHEDRON:
            elem[iElem] = new CHexahedron(Index[geometry->elem[jElem]->GetNode(0)],
                                          Index[geometry->elem[jElem]->GetNode(1)],
                                          Index[geometry->elem[jElem]->GetNode(2)],
                                          Index[geometry->elem[jElem]->GetNode(3)],
                                          Index[geometry->elem[jElem]->GetNode(4)],
                                          Index[geometry->elem[jElem]->GetNode(5)],
                                          Index[geometry->elem[jElem]->GetNode(6)],
                                          Index[geometry->elem[jElem]->GetNode(7)]);
            iElem++; nelem_hexa++;
            break;
            
          case PRISM:
            elem[iElem] = new CPrism(Index[geometry->elem[jElem]->GetNode(0)],
                                     Index[geometry->elem[jElem]->GetNode(1)],
                                     Index[geometry->elem[jElem]->GetNode(2)],
                                     Index[geometry->elem[jElem]->GetNode(3)],
                                     Index[geometry->elem[jElem]->GetNode(4)],
                                     Index[geometry->elem[jElem]->GetNode(5)]);
            iElem++; nelem_prism++;
            break;
            
          case PYRAMID:
            elem[iElem] = new CPyramid(Index[geometry->elem[jElem]->GetNode(0)],
                                       Index[geometry->elem[jElem]->GetNode(1)],
                                       Index[geometry->elem[jElem]->GetNode(2)],
                                       Index[geometry->elem[jElem]->GetNode(3)],
                                       Index[geometry->elem[jElem]->GetNode(4)]);
            iElem++; nelem_pyramid++;
            break;
            
        }
      }
    }
  }
  
  nElem = geometry->GetnElem() + nElem_new;
  
  /*--- Add the old points ---*/

  nPointNode = geometry->GetnPoint() + nPoint_new;
  node = new CPoint*[geometry->GetnPoint() + nPoint_new];
  for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint ++) {
    if (geometry->GetnDim() == 2)
      node[iPoint] = new CPoint(geometry->node[iPoint]->GetCoord(0),
                                geometry->node[iPoint]->GetCoord(1), iPoint, config);
    if (geometry->GetnDim() == 3)
      node[iPoint] = new CPoint(geometry->node[iPoint]->GetCoord(0),
                                geometry->node[iPoint]->GetCoord(1),
                                geometry->node[iPoint]->GetCoord(2), iPoint, config);
  }
  
  /*--- Add the new points due to the periodic boundary condtion (only in the mirror part) ---*/
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (CreateMirror[iPeriodic]) {
      for (iIndex = 0; iIndex < geometry->PeriodicPoint[iPeriodic][0].size(); iIndex++) {
        
        /*--- From iPeriodic obtain the iMarker ---*/
        for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++)
          if (iPeriodic == config->GetMarker_All_PerBound(iMarker)) break;
        
        /*--- Retrieve the supplied periodic information. ---*/
        center = config->GetPeriodicRotCenter(config->GetMarker_All_TagBound(iMarker));
        angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(iMarker));
        trans  = config->GetPeriodicTranslation(config->GetMarker_All_TagBound(iMarker));
        
        /*--- Store center - trans as it is constant and will be added on.
         Note the subtraction, as this is the inverse translation. ---*/
        translation[0] = center[0] - trans[0];
        translation[1] = center[1] - trans[1];
        translation[2] = center[2] - trans[2];
        
        /*--- Store angles separately for clarity. Compute sines/cosines.
         Note the negative sign, as this is the inverse rotation. ---*/
        theta = -angles[0];
        phi   = -angles[1];
        psi   = -angles[2];
        
        cosTheta = cos(theta);  cosPhi = cos(phi);  cosPsi = cos(psi);
        sinTheta = sin(theta);  sinPhi = sin(phi);  sinPsi = sin(psi);
        
        /*--- Compute the rotation matrix. Note that the implicit
         ordering is rotation about the x-axis, y-axis, then z-axis. ---*/
        rotMatrix[0][0] = cosPhi*cosPsi;
        rotMatrix[1][0] = cosPhi*sinPsi;
        rotMatrix[2][0] = -sinPhi;
        
        rotMatrix[0][1] = sinTheta*sinPhi*cosPsi - cosTheta*sinPsi;
        rotMatrix[1][1] = sinTheta*sinPhi*sinPsi + cosTheta*cosPsi;
        rotMatrix[2][1] = sinTheta*cosPhi;
        
        rotMatrix[0][2] = cosTheta*sinPhi*cosPsi + sinTheta*sinPsi;
        rotMatrix[1][2] = cosTheta*sinPhi*sinPsi - sinTheta*cosPsi;
        rotMatrix[2][2] = cosTheta*cosPhi;
        
        /*--- Retrieve node information for this boundary point. ---*/
        iPoint = geometry->PeriodicPoint[iPeriodic][0][iIndex];
        jPoint = geometry->PeriodicPoint[iPeriodic][1][iIndex];
        Coord_i = geometry->node[iPoint]->GetCoord();
        
        /*--- Get the position vector from rot center to point. ---*/
        dx = Coord_i[0] - center[0];
        dy = Coord_i[1] - center[1];
        if (nDim == 3) {
          dz = Coord_i[2] - center[2];
        } else {
          dz = 0.0;
        }
        
        /*--- Compute transformed point coordinates. ---*/
        rotCoord[0] = rotMatrix[0][0]*dx + rotMatrix[0][1]*dy + rotMatrix[0][2]*dz + translation[0];
        rotCoord[1] = rotMatrix[1][0]*dx + rotMatrix[1][1]*dy + rotMatrix[1][2]*dz + translation[1];
        rotCoord[2] = rotMatrix[2][0]*dx + rotMatrix[2][1]*dy + rotMatrix[2][2]*dz + translation[2];
        
        /*--- Save the new points with the new coordinates. ---*/
        if (geometry->GetnDim() == 2)
          node[jPoint] = new CPoint(rotCoord[0], rotCoord[1], jPoint, config);
        if (geometry->GetnDim() == 3)
          node[jPoint] = new CPoint(rotCoord[0], rotCoord[1], rotCoord[2], jPoint, config);
        
      }
    }
  }
  
  nPoint = geometry->GetnPoint() + nPoint_new;
  
  /*--- Add the old boundary, reserving space for two new bc (send/recive periodic bc) ---*/
  nMarker = geometry->GetnMarker() + 2;
  nElem_Bound = new unsigned long [nMarker];
  bound = new CPrimalGrid**[nMarker];
  Tag_to_Marker = new string [nMarker_Max];
  config->SetnMarker_All(nMarker);
  
  /*--- Copy the olf boundary ---*/
  for (iMarker = 0; iMarker < geometry->GetnMarker(); iMarker++) {
    
    bound[iMarker] = new CPrimalGrid* [geometry->GetnElem_Bound(iMarker)];
    
    for (iVertex = 0; iVertex < geometry->GetnElem_Bound(iMarker); iVertex++) {
      if (geometry->bound[iMarker][iVertex]->GetVTK_Type() == LINE)
        bound[iMarker][iVertex] = new CLine(geometry->bound[iMarker][iVertex]->GetNode(0),
                                            geometry->bound[iMarker][iVertex]->GetNode(1), 2);
      if (geometry->bound[iMarker][iVertex]->GetVTK_Type() == TRIANGLE)
        bound[iMarker][iVertex] = new CTriangle(geometry->bound[iMarker][iVertex]->GetNode(0),
                                                geometry->bound[iMarker][iVertex]->GetNode(1),
                                                geometry->bound[iMarker][iVertex]->GetNode(2), 3);
      if (geometry->bound[iMarker][iVertex]->GetVTK_Type() == QUADRILATERAL)
        bound[iMarker][iVertex] = new CQuadrilateral(geometry->bound[iMarker][iVertex]->GetNode(0),
                                                 geometry->bound[iMarker][iVertex]->GetNode(1),
                                                 geometry->bound[iMarker][iVertex]->GetNode(2),
                                                 geometry->bound[iMarker][iVertex]->GetNode(3), 3);
    }
    
    nElem_Bound[iMarker] = geometry->GetnElem_Bound(iMarker);
    Tag_to_Marker[iMarker] = geometry->GetMarker_Tag(iMarker);
    
  }
  
  delete [] Index;
  delete [] CreateMirror;
  
}

CPeriodicGeometry::~CPeriodicGeometry(void) {
  unsigned long iElem_Bound;
  unsigned short iMarker;
  
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
      if (newBoundPer[iMarker][iElem_Bound] != NULL) delete [] newBoundPer[iMarker][iElem_Bound];
    }
  }
  if (newBoundPer != NULL) delete[] newBoundPer;
  
  if (nNewElem_BoundPer != NULL) delete[] nNewElem_BoundPer;
  
}

void CPeriodicGeometry::SetPeriodicBoundary(CGeometry *geometry, CConfig *config) {
  unsigned short iMarker, iPeriodic, nPeriodic = 0, iMarkerSend, iMarkerReceive;
  unsigned long iVertex, Counter_Send = 0, Counter_Receive = 0, iIndex;
  
  /*--- Compute the number of periodic bc on the geometry ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY)
      nPeriodic++;
  
  /*--- First compute the Send/Receive boundaries, count the number of points ---*/
  Counter_Send = 0; 	Counter_Receive = 0;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    if (geometry->PeriodicPoint[iPeriodic][0].size() != 0)
      Counter_Send += geometry->PeriodicPoint[iPeriodic][0].size();
    if (geometry->PeriodicPoint[iPeriodic][1].size() != 0)
      Counter_Receive += geometry->PeriodicPoint[iPeriodic][1].size();
  }
  
  /*--- Adimensionalization of the new boundaries ---*/
  iMarkerSend = nMarker - 2; iMarkerReceive = nMarker - 1;
  config->SetMarker_All_SendRecv(iMarkerSend,1);
  config->SetMarker_All_SendRecv(iMarkerReceive,-1);
  nElem_Bound[iMarkerSend] = Counter_Send;
  nElem_Bound[iMarkerReceive] = Counter_Receive;
  bound[iMarkerSend] = new CPrimalGrid* [Counter_Send];
  bound[iMarkerReceive] = new CPrimalGrid* [Counter_Receive];
  
  /*--- First we do the send ---*/
  iVertex = 0;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++)
    if (geometry->PeriodicPoint[iPeriodic][0].size() != 0)
      for (iIndex = 0; iIndex < geometry->PeriodicPoint[iPeriodic][0].size(); iIndex++) {
        bound[iMarkerSend][iVertex] = new CVertexMPI(geometry->PeriodicPoint[iPeriodic][0][iIndex], nDim);
        bound[iMarkerSend][iVertex]->SetRotation_Type(iPeriodic);
        iVertex++;
      }
  
  /*--- Second we do the receive ---*/
  iVertex = 0;
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++)
    if (geometry->PeriodicPoint[iPeriodic][1].size() != 0)
      for (iIndex = 0; iIndex < geometry->PeriodicPoint[iPeriodic][1].size(); iIndex++) {
        bound[iMarkerReceive][iVertex] = new CVertexMPI(geometry->PeriodicPoint[iPeriodic][1][iIndex], nDim);
        bound[iMarkerReceive][iVertex]->SetRotation_Type(iPeriodic);
        iVertex++;
      }
  
}

void CPeriodicGeometry::SetMeshFile(CGeometry *geometry, CConfig *config, string val_mesh_out_filename) {
  unsigned long iElem, iPoint, iElem_Bound, GhostPoints;
  unsigned short iMarker, iNodes, iDim;
  unsigned short iMarkerReceive, iPeriodic, nPeriodic = 0;
  ofstream output_file;
  string Grid_Marker;
  char *cstr;
  su2double *center, *angles, *transl;
  
  cstr = new char [val_mesh_out_filename.size()+1];
  strcpy (cstr, val_mesh_out_filename.c_str());
  
  /*--- Open .su2 grid file ---*/
  output_file.precision(15);
  output_file.open(cstr, ios::out);
  
  /*--- Ghost points, look at the nodes in the send receive ---*/
  iMarkerReceive = nMarker - 1;
  GhostPoints = nElem_Bound[iMarkerReceive];
  
  /*--- Change the numbering to guarantee that the all the receive
   points are at the end of the file ---*/
  unsigned long OldnPoint = geometry->GetnPoint();
  unsigned long *NewSort = new unsigned long[nPoint];
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    NewSort[iPoint] = iPoint;
  }
  
  unsigned long Index = OldnPoint-1;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (bound[iMarker][0]->GetVTK_Type() == VERTEX) {
      if (config->GetMarker_All_SendRecv(iMarker) < 0) {
        for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
          if (bound[iMarker][iElem_Bound]->GetNode(0) < geometry->GetnPoint()) {
            NewSort[bound[iMarker][iElem_Bound]->GetNode(0)] = Index;
            NewSort[Index] = bound[iMarker][iElem_Bound]->GetNode(0);
            Index--;
          }
        }
      }
    }
  }
  
  
  /*--- Write dimension, number of elements and number of points ---*/
  output_file << "NDIME= " << nDim << endl;
  output_file << "NELEM= " << nElem << endl;
  for (iElem = 0; iElem < nElem; iElem++) {
    output_file << elem[iElem]->GetVTK_Type();
    for (iNodes = 0; iNodes < elem[iElem]->GetnNodes(); iNodes++)
      output_file << "\t" << NewSort[elem[iElem]->GetNode(iNodes)];
    output_file << "\t"<<iElem<< endl;
  }
  
  output_file << "NPOIN= " << nPoint << "\t" << nPoint - GhostPoints << endl;
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++)
      output_file << scientific << "\t" << node[NewSort[iPoint]]->GetCoord(iDim) ;
    output_file << "\t" << iPoint << endl;
  }
  
  output_file << "NMARK= " << nMarker << endl;
  for (iMarker = 0; iMarker < nMarker; iMarker++) {
    if (bound[iMarker][0]->GetVTK_Type() != VERTEX) {
      
      Grid_Marker = config->GetMarker_All_TagBound(iMarker);
      output_file << "MARKER_TAG= " << Grid_Marker << endl;
      output_file << "MARKER_ELEMS= " << nElem_Bound[iMarker] + nNewElem_BoundPer[iMarker] << endl;
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        output_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
        for (iNodes = 0; iNodes < bound[iMarker][iElem_Bound]->GetnNodes()-1; iNodes++)
          output_file << NewSort[bound[iMarker][iElem_Bound]->GetNode(iNodes)] << "\t" ;
        iNodes = bound[iMarker][iElem_Bound]->GetnNodes()-1;
        output_file << NewSort[bound[iMarker][iElem_Bound]->GetNode(iNodes)] << endl;
      }
      
      /*--- Write any new elements at the end of the list. ---*/
      if (nNewElem_BoundPer[iMarker] > 0) {
        for (iElem_Bound = 0; iElem_Bound < nNewElem_BoundPer[iMarker]; iElem_Bound++) {
          output_file << newBoundPer[iMarker][iElem_Bound]->GetVTK_Type() << "\t" ;
          for (iNodes = 0; iNodes < newBoundPer[iMarker][iElem_Bound]->GetnNodes()-1; iNodes++)
            output_file << NewSort[newBoundPer[iMarker][iElem_Bound]->GetNode(iNodes)] << "\t" ;
          iNodes = newBoundPer[iMarker][iElem_Bound]->GetnNodes()-1;
          output_file << NewSort[newBoundPer[iMarker][iElem_Bound]->GetNode(iNodes)] << endl;
        }
      }
      
    }
    
    if (bound[iMarker][0]->GetVTK_Type() == VERTEX) {
      output_file << "MARKER_TAG= SEND_RECEIVE" << endl;
      output_file << "MARKER_ELEMS= " << nElem_Bound[iMarker]<< endl;
      if (config->GetMarker_All_SendRecv(iMarker) > 0) output_file << "SEND_TO= " << config->GetMarker_All_SendRecv(iMarker) << endl;
      if (config->GetMarker_All_SendRecv(iMarker) < 0) output_file << "SEND_TO= " << config->GetMarker_All_SendRecv(iMarker) << endl;
      
      for (iElem_Bound = 0; iElem_Bound < nElem_Bound[iMarker]; iElem_Bound++) {
        output_file << bound[iMarker][iElem_Bound]->GetVTK_Type() << "\t" <<
        NewSort[bound[iMarker][iElem_Bound]->GetNode(0)] << "\t" <<
        bound[iMarker][iElem_Bound]->GetRotation_Type()  << endl;
      }
    }
  }
  
  /*--- Compute the number of periodic bc on the geometry ---*/
  for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
    if (config->GetMarker_All_KindBC(iMarker) == PERIODIC_BOUNDARY)
      nPeriodic++;
  
  output_file << "NPERIODIC= " << nPeriodic + 1 << endl;
  
  /*--- Periodic 0 correspond with no movement of the surface ---*/
  output_file << "PERIODIC_INDEX= 0" << endl;
  output_file << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << endl;
  output_file << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << endl;
  output_file << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << "\t" << "0.000000000000000e+00" << endl;
  
  /*--- From iPeriodic obtain the iMarker ---*/
  for (iPeriodic = 1; iPeriodic <= nPeriodic; iPeriodic++) {
    for (iMarker = 0; iMarker < nMarker; iMarker++)
      if (iPeriodic == config->GetMarker_All_PerBound(iMarker)) break;
    
    /*--- Retrieve the supplied periodic information. ---*/
    center = config->GetPeriodicRotCenter(config->GetMarker_All_TagBound(iMarker));
    angles = config->GetPeriodicRotAngles(config->GetMarker_All_TagBound(iMarker));
    transl = config->GetPeriodicTranslation(config->GetMarker_All_TagBound(iMarker));
    
    output_file << "PERIODIC_INDEX= " << iPeriodic << endl;
    output_file << center[0] << "\t" << center[1] << "\t" << center[2] << endl;
    output_file << angles[0] << "\t" << angles[1] << "\t" << angles[2] << endl;
    output_file << transl[0] << "\t" << transl[1] << "\t" << transl[2] << endl;
    
  }
  
  
  output_file.close();
  
  /*--- Free memory ---*/
  delete [] NewSort;
  
}

void CPeriodicGeometry::SetTecPlot(char mesh_filename[MAX_STRING_SIZE], bool new_file) {
  
  unsigned long iElem, iPoint;
  unsigned short iDim;
  ofstream Tecplot_File;
  
  Tecplot_File.open(mesh_filename, ios::out);
  Tecplot_File << "TITLE= \"Visualization of the volumetric grid\"" << endl;
  
  if (nDim == 2) {
    Tecplot_File << "VARIABLES = \"x\",\"y\" " << endl;
    Tecplot_File << "ZONE NODES= "<< nPoint <<", ELEMENTS= "<< nElem <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
  }
  if (nDim == 3) {
    Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\" " << endl;
    Tecplot_File << "ZONE NODES= "<< nPoint <<", ELEMENTS= "<< nElem <<", DATAPACKING=POINT, ZONETYPE=FEBRICK"<< endl;
  }
  
  for (iPoint = 0; iPoint < nPoint; iPoint++) {
    for (iDim = 0; iDim < nDim; iDim++)
      Tecplot_File << scientific << node[iPoint]->GetCoord(iDim) << "\t";
    Tecplot_File << "\n";
  }
  
  for (iElem = 0; iElem < nElem; iElem++) {
    if (elem[iElem]->GetVTK_Type() == TRIANGLE) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(2)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == QUADRILATERAL) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == TETRAHEDRON) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(2)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(3)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == HEXAHEDRON) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(5)+1 <<" "<<
      elem[iElem]->GetNode(6)+1 <<" "<< elem[iElem]->GetNode(7)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == PYRAMID) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(2)+1 <<" "<< elem[iElem]->GetNode(3)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(4)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(4)+1 << endl;
    }
    if (elem[iElem]->GetVTK_Type() == PRISM) {
      Tecplot_File <<
      elem[iElem]->GetNode(0)+1 <<" "<< elem[iElem]->GetNode(1)+1 <<" "<<
      elem[iElem]->GetNode(1)+1 <<" "<< elem[iElem]->GetNode(2)+1 <<" "<<
      elem[iElem]->GetNode(3)+1 <<" "<< elem[iElem]->GetNode(4)+1 <<" "<<
      elem[iElem]->GetNode(4)+1 <<" "<< elem[iElem]->GetNode(5)+1 << endl;
    }
  }
  
  Tecplot_File.close();
}

CMultiGridQueue::CMultiGridQueue(unsigned long val_npoint) {
  unsigned long iPoint;
  
  nPoint = val_npoint;
  Priority = new short[nPoint];
  RightCV = new bool[nPoint];
  
  QueueCV.resize(1);
  
  /*--- Queue initialization with all the points in the finer grid ---*/
  for (iPoint = 0; iPoint < nPoint; iPoint ++) {
    QueueCV[0].push_back(iPoint);
    Priority[iPoint] = 0;
    RightCV[iPoint] = true;
  }
  
}

CMultiGridQueue::~CMultiGridQueue(void) {
  
  delete[] Priority;
  delete[] RightCV;
  
}

void CMultiGridQueue::AddCV(unsigned long val_new_point, unsigned short val_number_neighbors) {
  
  unsigned short Max_Neighbors = QueueCV.size()-1;
  
  /*--- Basic check ---*/
  if (val_new_point > nPoint) {
    cout << "The index of the CV is greater than the size of the priority list." << endl;
    exit(EXIT_FAILURE);
  }
  
  /*--- Resize the list ---*/
  if (val_number_neighbors > Max_Neighbors)
    QueueCV.resize(val_number_neighbors+1);
  
  /*--- Find the point in the queue ---*/
  bool InQueue = false;
  if (Priority[val_new_point] == val_number_neighbors) InQueue = true;
  
  if (!InQueue) {
    /*--- Add the control volume, and update the priority list ---*/
    QueueCV[val_number_neighbors].push_back(val_new_point);
    Priority[val_new_point] = val_number_neighbors;
  }
  
}

void CMultiGridQueue::RemoveCV(unsigned long val_remove_point) {
  unsigned short iPoint;
  bool check;
  
  /*--- Basic check ---*/
  if (val_remove_point > nPoint) {
    cout << "The index of the CV is greater than the size of the priority list." << endl;
    exit(EXIT_FAILURE);
  }
  
  /*--- Find priority of the Control Volume ---*/
  short Number_Neighbors = Priority[val_remove_point];
  if (Number_Neighbors == -1) {
    cout << "The CV "<< val_remove_point <<" is not in the priority list. (RemoveCV)" << endl;
    exit(EXIT_FAILURE);
  }
  
  /*--- Find the point in the queue ---*/
  vector<unsigned long>::iterator ItQueue = find(QueueCV[Number_Neighbors].begin(),
                                                 QueueCV[Number_Neighbors].end(),
                                                 val_remove_point);
  if ( ItQueue != QueueCV[Number_Neighbors].end() ) QueueCV[Number_Neighbors].erase(ItQueue);
  
  Priority[val_remove_point] = -1;
  
  /*--- Check that the size of the queue is the right one ---*/
  unsigned short Size_QueueCV = 0;
  check = false;
  for (iPoint = 0; iPoint < QueueCV.size(); iPoint ++)
    if (QueueCV[iPoint].size() != 0) { Size_QueueCV = iPoint; check = true;}
  
  /*--- Resize the queue, if check = false, the queue is empty, at least
   we need one element in the queue ---*/
  if (check) QueueCV.resize(Size_QueueCV+1);
  else QueueCV.resize(1);
  
}

void CMultiGridQueue::MoveCV(unsigned long val_move_point, short val_number_neighbors) {
  
  if (val_number_neighbors < 0) {
    val_number_neighbors = 0;
    RightCV[val_move_point] = false;
  }
  else {
    RightCV[val_move_point] = true;
  }
  
  /*--- Remove the control volume ---*/
  RemoveCV(val_move_point);
  
  /*--- Add a new control volume ---*/
  AddCV(val_move_point, val_number_neighbors);
  
}

void CMultiGridQueue::IncrPriorityCV(unsigned long val_incr_point) {
  
  /*--- Find the priority list ---*/
  short Number_Neighbors = Priority[val_incr_point];
  if (Number_Neighbors == -1) {
    cout << "The CV "<< val_incr_point <<" is not in the priority list. (IncrPriorityCV)" << endl;
    exit(EXIT_FAILURE);
  }
  
  /*--- Remove the control volume ---*/
  RemoveCV(val_incr_point);
  
  /*--- Increase the priority ---*/
  AddCV(val_incr_point, Number_Neighbors+1);
  
}

void CMultiGridQueue::RedPriorityCV(unsigned long val_red_point) {
  
  /*--- Find the priority list ---*/
  short Number_Neighbors = Priority[val_red_point];
  if (Number_Neighbors == -1) {
    cout << "The CV "<< val_red_point <<" is not in the priority list. (RedPriorityCV)" << endl;
    exit(EXIT_FAILURE);
  }
  
  if (Number_Neighbors != 0) {
    
    /*--- Remove the control volume ---*/
    RemoveCV(val_red_point);
    
    /*--- Increase the priority ---*/
    AddCV(val_red_point, Number_Neighbors-1);
    
  }
  
}

void CMultiGridQueue::VisualizeQueue(void) {
  unsigned short iPoint;
  unsigned long jPoint;
  
  cout << endl;
  for (iPoint = 0; iPoint < QueueCV.size(); iPoint ++) {
    cout << "Number of neighbors " << iPoint <<": ";
    for (jPoint = 0; jPoint < QueueCV[iPoint].size(); jPoint ++) {
      cout << QueueCV[iPoint][jPoint] << " ";
    }
    cout << endl;
  }
  
}

void CMultiGridQueue::VisualizePriority(void) {
  unsigned long iPoint;
  
  for (iPoint = 0; iPoint < nPoint; iPoint ++)
    cout << "Control Volume: " << iPoint <<" Priority: " << Priority[iPoint] << endl;
  
}

long CMultiGridQueue::NextCV(void) {
  if (QueueCV.size() != 0) return QueueCV[QueueCV.size()-1][0];
  else return -1;
}

bool CMultiGridQueue::EmptyQueue(void) {
  unsigned short iPoint;
  
  /*--- In case there is only the no agglomerated elements,
   check if they can be agglomerated or we have already finished ---*/
  bool check = true;
  
  if ( QueueCV.size() == 1 ) {
    for (iPoint = 0; iPoint < QueueCV[0].size(); iPoint ++) {
      if (RightCV[QueueCV[0][iPoint]]) { check = false; break; }
    }
  }
  else {
    for (iPoint = 1; iPoint < QueueCV.size(); iPoint ++)
      if (QueueCV[iPoint].size() != 0) { check = false; break;}
  }
  
  return check;
}

unsigned long CMultiGridQueue::TotalCV(void) {
  unsigned short iPoint;
  unsigned long TotalCV;
  
  TotalCV = 0;
  for (iPoint = 0; iPoint < QueueCV.size(); iPoint ++)
    if (QueueCV[iPoint].size() != 0) { TotalCV += QueueCV[iPoint].size(); }
  
  return TotalCV;
}

void CMultiGridQueue::Update(unsigned long iPoint, CGeometry *fine_grid) {
  unsigned short iNode;
  unsigned long jPoint;
  
  RemoveCV(iPoint);
  for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
    jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
    if (fine_grid->node[jPoint]->GetAgglomerate() == false)
      IncrPriorityCV(jPoint);
  }
  
}
