'''Convenience functions for generating platonic solids
   See also platonic.h'''

from __future__ import (division,absolute_import)
from other.core import *

def sphere_mesh(refinements,center=0,radius=1):
  return sphere_mesh_py(refinements,center,radius)

def tetrahedron_mesh():
  X = array([[ 1, 1, 1],[-1,-1, 1],[-1,+1,-1],[+1,-1,-1]],dtype=real)
  mesh = TriangleMesh([[1,2,3],[3,2,0],[3,0,1],[0,2,1]])
  return mesh,X

def cube_mesh():
  X = array([[0,0,0],[0,0,1],[0,1,0],[0,1,1],[1,0,0],[1,0,1],[1,1,0],[1,1,1]],dtype=real)
  mesh = TriangleMesh([[0,1,2], [2,1,3], 
                       [1,0,5], [5,0,4], 
                       [3,1,7], [7,1,5], 
                       [0,2,4], [4,2,6], 
                       [2,3,6], [6,3,7], 
                       [5,6,7], [6,5,4]])
  return mesh,X

def circle_mesh(n,center=0,radius=1):
  i = arange(n,dtype=int32)
  segments = empty((n,2),dtype=int32)
  segments[:,0] = i
  segments[:-1,1] = i[1:]
  segments[-1,1] = 0
  mesh = SegmentMesh(segments)
  if center is None:
    return mesh
  theta = 2*pi/n*i
  return mesh,(radius*vstack([cos(theta),sin(theta)])).T.copy() 
