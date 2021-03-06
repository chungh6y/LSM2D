# mymain.py

import numpy as np
import scipy as sp
import LinE as LinE # module of linE of my own
import Sensitivity_vec as Sens
from slsm_Module import *
import time as clock_t
#from ctypes import *
import matplotlib.pyplot as plt

#from openmdao.api import Component, Group, IndepVarComp, Problem

# 0. setup
AllowedAreaFraction = 0.01
moveLimit = 0.9
maxTime = 200
minArea = 0.5 
VoidMaterial = 1e-6
 
# 1. Define domains
lxy = [160,80]
exy = [160,80]
CMesh = LinE.FEAMeshQ4(lxy,exy)
mesh  = slsm.Mesh(exy[0],exy[1],False)
meshArea = exy[0]*exy[1]

Holes = slsm.vector__Holes__()
Holes.append(slsm.Hole(16, 14, 5))
Holes.append(slsm.Hole(32, 27, 5))
Holes.append(slsm.Hole(48, 14, 5))
Holes.append(slsm.Hole(64, 27, 5))
Holes.append(slsm.Hole(80, 14, 5))
Holes.append(slsm.Hole(96, 27, 5))
Holes.append(slsm.Hole(112, 14, 5))
Holes.append(slsm.Hole(128, 27, 5))
Holes.append(slsm.Hole(144, 14, 5))
Holes.append(slsm.Hole(16, 40, 5))
Holes.append(slsm.Hole(32, 53, 5))
Holes.append(slsm.Hole(48, 40, 5))
Holes.append(slsm.Hole(64, 53, 5))
Holes.append(slsm.Hole(80, 40, 5))
Holes.append(slsm.Hole(96, 53, 5))
Holes.append(slsm.Hole(112, 40, 5))
Holes.append(slsm.Hole(128, 53, 5))
Holes.append(slsm.Hole(144, 40, 5))
Holes.append(slsm.Hole(16, 66, 5))
Holes.append(slsm.Hole(48, 66, 5))
Holes.append(slsm.Hole(80, 66, 5))
Holes.append(slsm.Hole(112, 66, 5))
Holes.append(slsm.Hole(144, 66, 5))

levelSet = slsm.LevelSet(mesh,Holes,moveLimit,6,False) # moveLimit is defined elsewhere
levelSet.reinitialise()

# 1.1. Volume fraction
for i in range(0,CMesh.nELEM):
    if mesh.elements[i].area < VoidMaterial:
        CMesh.AreaFraction[i] = VoidMaterial
    else:
        CMesh.AreaFraction[i] = mesh.elements[i].area

boundary = slsm.Boundary(levelSet)
boundary.discretise(False)
boundary.computeAreaFractions()
boundary.ComputeNormalVectors()

io = slsm.InputOutput()

# 2. Lienar Elasticity
E = 1.0 
v = 0.3
thickness = 1.0

Cijkl = LinE.LinearElasticMaterial.get_Cijkl_E_v(E,v)

CLinElasticity = LinE.LinearElasticity(CMesh,Cijkl)
CSensitivities = Sens.ElasticitySensitivities(CLinElasticity)

for ii in range(0,CMesh.nELEM):
    if (mesh.elements [ii].area < VoidMaterial):
        CMesh.AreaFraction[ii] = VoidMaterial
    else:
        CMesh.AreaFraction[ii] = mesh.elements [ii].area #VERIFIED
        
# 4. 1st nest for boundary evolution

Radius = 2
Weights = 1
AllowedAreaFraction = 0.01
WeightFlag = 5
nReinit = 0

meshArea = exy[0]*exy[1]
Max_Iter = 100

# results 
times = [0]
time = 0
Objectives = [None]
areas = [None]

#lambdas = vector__double__()
#lambdas.append(0)
#lambdas.append(0)

Multipliers = vector__double__()
Multipliers.append(0)
#%%
t = 0.0
for iIter in range(0,Max_Iter):
    #%%
  # =========== LINEAR ELASTICITY MODULE ===============================================
    # t = clock_t.time()
    CLinElasticity.Assembly() 
    #print("Assembly takes %d sec\n",clock_t.time()-t)
    # BCs 
    Xlo_id = CMesh.get_NodeID([0,0],1e-3,np.inf)
    BC_fixed = CMesh.get_dof('xy',Xlo_id)

    xtip1 = CMesh.get_NodeID([lxy[0],int(lxy[1]/2)],1e-3,1e-3)
    xtip2 = CMesh.get_NodeID([lxy[0],int(lxy[1]/2)-1],1e-3,1e-3)
    xtip3 = CMesh.get_NodeID([lxy[0],int(lxy[1]/2)+1],1e-3,1e-3)

    BC_force1 = CMesh.get_dof('y',xtip1)
    BC_force2 = CMesh.get_dof('y',xtip2)
    BC_force3 = CMesh.get_dof('y',xtip3)

    CLinElasticity.Apply_BC(BC_fixed)

    CLinElasticity.set_F()
    CLinElasticity.set_F(BC_force1,-5)
    CLinElasticity.set_F(BC_force2,-2.5)
    CLinElasticity.set_F(BC_force3,-2.5)

    # t = clock_t.time()
    u = CLinElasticity.solve()
#    compliance = CLinElasticity.get_compliance(u) #VERIFIED
    #print("Linear solver takes %d sec\n",clock_t.time()-t)
    #%%
    nBpts = len(boundary.points)
    BoundaryPoints = np.zeros([nBpts,2])
    
    
    for ii in range(0,nBpts):
        BoundaryPoints[ii,0] = boundary.points[ii].coord.x
        BoundaryPoints[ii,1] = boundary.points[ii].coord.y #VERIFIED

    # t = clock_t.time()
    BoundarySensitivities = CSensitivities.Compliance(BoundaryPoints, Weights, Radius, WeightFlag, AllowedAreaFraction)
    #print("bpt sensitivity calculation takes %d sec\n",clock_t.time()-t)

    np.savetxt("txts/BoundarySensitivities_%i.txt" %iIter, BoundarySensitivities, delimiter = ' ')

    plt.clf()
    plt.scatter(BoundaryPoints[:,0],BoundaryPoints[:,1], 20, BoundarySensitivities, marker = 'o')
    plt.savefig("plots/bndSens%d.png" %iIter)

    plt.clf()
    plt.plot(BoundaryPoints[:,0],BoundaryPoints[:,1],'o')
    plt.savefig("plots/bndPoints_%d.png" %iIter)

        

    for ii in range(0,nBpts):
        boundary.points[ii].sensitivities[0] = BoundarySensitivities[ii] #VERIFIED
        boundary.points[ii].sensitivities[1] = -1.0
    
    #%%
    constraintDistances = vector__double__()
    constraintDistances.append(meshArea*minArea - boundary.area)
    
    ## should be changed to MDAO =============================
#    #print("Attempt to optimise\n")    
#    timeStep = float()
#    optimise = slsm.Optimise(boundary.points, constraintDistances, lambdas, Multipliers, timeStep)
#    optimise.solve()
    a = slsm_Module()
    lambdas = vector__double__()
    lambdas.append(0)
    lambdas.append(0)

   
    # t = clock_t.time()
    timeStep = a.wrapper_optimise(boundary.points, constraintDistances, lambdas, Multipliers, levelSet.moveLimit)
#    #print("optimization takes %d sec\n",clock_t.time()-t)
    plt.clf()
    plt.plot(BoundaryPoints[:,0],BoundaryPoints[:,1],'o')
    plt.savefig("bndPoints_%d.png" %iIter)


    #%%

    # t = clock_t.time()
    levelSet.computeVelocities(boundary.points)
    levelSet.ComputeGradients()
   
    phi = np.zeros((lxy[0]+1)*(lxy[1]+1))
    vel = np.zeros((lxy[0]+1)*(lxy[1]+1))
    for ii in range(0,len(phi)):
        phi[ii] = levelSet.signedDistance[ii]
	vel[ii] = levelSet.velocity[ii]

    np.savetxt('txts/signedDistance_%i.txt' %iIter, phi, delimiter = ' ')
    np.savetxt('txts/velocities_%i.txt' %iIter, vel, delimiter = ' ')
 
    #plt.clf()
    #plt.plot(CMesh.Nodes[:,0], CMesh.Nodes[:,1] , 20, vel, marker = 'o')
    #plt.savefig("plots/velocity_%d.png" %iIter)

    isReinitialised = levelSet.update(1)
    
    #if isReinitialised == False:
    #    if nReinit == 20:
    #    # levelSet.reinitialise()
    #nReinit += 1
    #else:
    #    nReinit = 0
    
    nReinit += 1

    boundary.discretise(False)
    boundary.computeAreaFractions()
    

    # export Area fraction to FEA
    # same as 1.1. Volume fraction
    for ii in range(0,CMesh.nELEM):
        if (mesh.elements [ii].area < VoidMaterial):
            CMesh.AreaFraction[ii] = VoidMaterial
        else:
            CMesh.AreaFraction[ii] = mesh.elements [ii].area
            
    boundary.ComputeNormalVectors()
    time += timeStep
    
    times.append(time)
#    #print("update takes %d sec\n",clock_t.time()-t)
    compliance = CLinElasticity.get_compliance(u)
    print(iIter, lambdas[0], lambdas[1], compliance, boundary.area/meshArea)
    
#    io.saveLevelSetVTK(times.size(), levelSet); #TOFIX
#    io.saveBoundarySegmentsTXT(times.size(), boundary); #TOFIX
    
    Objectives.append(compliance)
    areas.append(boundary.area/meshArea)
    

U2 = CLinElasticity.Field.reshape(CMesh._dpn,int(CMesh.nNODE),order='F').transpose()
node_f = CMesh.Nodes + U2

idE = CMesh.Elements[:,[0,1,2,3,0]].flatten(order='C').astype(int)
xorder = node_f[idE,0].reshape(int(len(idE)/(CMesh._npe+1)),CMesh._npe+1)
yorder = node_f[idE,1].reshape(int(len(idE)/(CMesh._npe+1)),CMesh._npe+1)
plt.plot(xorder.transpose(),yorder.transpose(),'b-')
