/*
  Copyright (c) 2015-2016 Lester Hedges <lester.hedges+lsm@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/* changed a bit to make it exposed to cython: HAC*/

/*! \file Optimise.cpp
    \brief A class for finding the solution for the optimum velocity vector.
 */
#include "./../include/optimise_noNLOPT.h"

Optimise::Optimise(std::vector<BoundaryPoint>& boundaryPoints_,
    const std::vector<double>& constraintDistances_,
    std::vector<double>& lambdas_,
    double& timeStep_,
    double maxDisplacement_,
    bool isMax_,
    bool isCapped_,
    const std::vector<bool>& isEquality_) :
    boundaryPoints(boundaryPoints_),
    constraintDistances(constraintDistances_),
    lambdas(lambdas_),
    timeStep(timeStep_),
    maxDisplacement(maxDisplacement_),
    isMax(isMax_),
    isCapped(isCapped_),
    isEquality(isEquality_)
    {
errno = EINVAL;
lsm_check(((maxDisplacement_ > 0) && (maxDisplacement_ <= 1)), "Move limit must be between 0 and 1.");

// Store the initial number of constraints.
nConstraints = lambdas.size() - 1;
nConstraintsInitial = nConstraints;

// Check for empty constraint distances vector.
if (constraintDistances.empty())
{
errno = 0;
lsm_log_warn("Empty constraint distances vector. Assuming unconstrained optimisation.");
nConstraints = 0;
}

// Resize data structures.
negativeLambdaLimits.resize(nConstraints + 1);
positiveLambdaLimits.resize(nConstraints + 1);
scaleFactors.resize(nConstraints + 1);

// Copy constraint distances.
constraintDistancesScaled = constraintDistances;

// ADD BY HAC
nPoints = boundaryPoints.size();
indexMap.clear();
indexMap.push_back(0);
indexMap.push_back(1); // FIXTHIS: TAG_indexmap

displacements.resize(nPoints);
isSideLimit.resize(nPoints);

// Set default as inequality constraints.
if (isEquality.size() == 0) isEquality.resize(nConstraints, false);

return;

error:
exit(EXIT_FAILURE);
}


double Optimise::callback(const std::vector<double>& lambda, std::vector<double>& gradient, unsigned int index)
{
    // Calculate the boundary displacement vector.
    computeDisplacements(lambda);   
    
    // Compute the gradients.
    if (!gradient.empty())
        computeGradients(lambda, gradient, index);

    // Return the function value.
    return computeFunction(index);
}

// double Optimise::solve()
// {
//     // Store the number of boundary points.
//     // This can change between successive optimisation calls.
//     nPoints = boundaryPoints.size();

//     // Resize boundary point dependent data structures.
//     displacements.resize(nPoints);
//     isSideLimit.resize(nPoints);

//     // Compute scaled constraint change distances and remove inactive
//     // inequality constraints.
//     computeConstraintDistances();

//     // Compute the scale factors for the objective and constraints.
//     computeScaleFactors();

//     // Scale inital lambda estimates.
//     for (unsigned int i=0;i<nConstraints+1;i++)
//         lambdas[i] /= scaleFactors[i];

//     // Compute the lambda limits.
//     computeLambdaLimits();

//     // Create wrapper for objective.
//     NLoptWrapper objectiveWrapper;
//     objectiveWrapper.index = 0;
//     objectiveWrapper.callback = this;

//     // Create wrappers for constraints.
//     NLoptWrapper constraintWrappers[nConstraints];
//     for (unsigned int i=0;i<nConstraints;i++)
//     {
//         constraintWrappers[i].index = i + 1;
//         constraintWrappers[i].callback = this;
//     }

//     // Instantiate NLopt optimisation object.
//     nlopt::opt opt(algorithm, 1 + nConstraints);

//     // Set limits.
//     opt.set_lower_bounds(negativeLambdaLimits);
//     opt.set_upper_bounds(positiveLambdaLimits);

//     // Set a maximum of 500 iterations.
//     opt.set_maxeval(500);

//     // Set convergence tolerance.
//     opt.set_xtol_rel(1e-8);

//     // Specify whether we want to minimise or maximise the objective function.
//     if (isMax) opt.set_max_objective(callbackWrapper, &objectiveWrapper);
//     else       opt.set_min_objective(callbackWrapper, &objectiveWrapper);

//     // Add the constraints.
//     for (unsigned int i=0;i<nConstraints;i++)
//     {
//         // Add equality constraint.
//         if (isEquality[i])
//             opt.add_equality_constraint(callbackWrapper, &constraintWrappers[i], 1e-8);

//         // Add inequality constraint.
//         else
//             opt.add_inequality_constraint(callbackWrapper, &constraintWrappers[i], 1e-8);
//     }

//     // The optimum value of the objective function.
//     double optObjective;

//     // Whether a single optimisation has been attempted.
//     bool isAttempted = false;

//     // Keep trying optimisation until a solution is found.
//     while (!isAttempted || (returnCode < 0))
//     {
//         isAttempted = true;

//         // Attempt optimisation.
//         try
//         {
//             returnCode = opt.optimize(lambdas, optObjective);
//         }

//         // Catch roundoff errors.
//         catch (nlopt::roundoff_limited)
//         {
//             // Reduce the constraint change targets.
//             for (unsigned int i=0;i<nConstraints;i++)
//                 constraintDistancesScaled[i] *= 0.7;
//         }

//         // Catch argument errors.
//         catch (std::invalid_argument)
//         {
//             errno = EINVAL;
//             lsm_log_err("Invalid arguments!");
//         }
//     }

//     // Compute the optimum displacement vector.
//     computeDisplacements(lambdas);

//     // Rescale the displacements and lambda values (if necessary).
//     optObjective *= rescaleDisplacements();

//     //
//     // LaplacianDisplacementsFilter(1);
//     //

//     // Calculate the unscaled lambda values.
//     for (unsigned int i=0;i<nConstraints+1;i++)
//         lambdas[i] *= scaleFactors[i];

//     // Effective time step.
//     // timeStep = std::abs(lambdas[0]);
//     timeStep = 0.05*std::abs(lambdas[0]);
//     // timeStep = 0.001;

//     // Calculate boundary point velocities.
//     for (unsigned int i=0;i<nPoints;i++)
//         boundaryPoints[i].velocity = displacements[i] / timeStep;

//     // Remap data if there are inactive constraints.
//     if (nConstraints < nConstraintsInitial)
//     {
//         // Return the lambda vector to its original size.
//         lambdas.resize(1 + nConstraintsInitial);

//         // Check for constraints.
//         if (nConstraints > 0)
//         {
//             // Copy the lambda values back into position.
//             // Loop backwards to avoid overwriting data.
//             for (unsigned int i=nConstraints+1;i>0;i--)
//                 lambdas[indexMap[i]] = lambdas[i];
//         }
//     }

//     // Return the unscaled objective change.
//     return (optObjective / scaleFactors[0]);
// }

void Optimise::computeScaleFactors()
{
    /* In order for the optimiser to work effectively it is important
       that small changes in the lambdas result in small changes in the
       functions for the objective and constraints and their respective
       gradients. Since we are solving a multi-dimensional optimisation
       problem it is important that all variables are on the same scale.
       This enables us to use a universal convergence tolerance.

       Our scaling protocol is described below. Note that we choose to
       store scale factors for each function, rather than scaling (then
       rescaling) the input data. Sensitivites are scaled down (reduced)
       and the lambda values are scale up (enlarged).

        1) For each function, scale by the largest absolute sensitivity,
           i.e. the maximum magnitude is one.

        2) Scale by the gradient at the origin (all lambdas are zero).
           This ensures that the gradient is close to one.

       There is no need to independently scale the boundary integral
       coefficients since the boundary lengths are independent of the
       function, i.e.

         c^f_i = s^f_i * l_i

       The l_i variables are the same for the objective and constraints
       so, by definition, the c^f's are on the same scale if we simply
       normalise by max(abs(s^f_i)).
     */
    //HAC std::cout << "nConstraints:" << nConstraints << "\n";
    //HAC std::cout << "nPoints:" << nPoints << "\n";
    // Loop over all functions: objective first, then constraints.
    for (unsigned int i=0;i<nConstraints+1;i++)
    {
        // Initialise maximum sensitivity.
        double maxSens = 0;
        double minSens = 0;

        // Loop over all boundary points.
        for (unsigned int j=0;j<nPoints;j++)
        {
            // Don't consider fixed points.
            if (!boundaryPoints[j].isFixed)
            {
                // Test whether sensitivity magnitude is current maximum.
                // std::cout << "sensitivity of " << j << "of " << i <<"\n";
                double sens = std::abs(boundaryPoints[j].sensitivities[i]);
                if (sens > maxSens) maxSens = sens;

                double sens2 = boundaryPoints[j].sensitivities[i];
                if (sens2 < minSens) minSens = sens2;
            }
        }
        
        // Store scale factor.
        scaleFactors[i] = (1.0 / maxSens);
    }

    //HAC std::cout << "end loop of scalefactor #1" << "\n";
    // Create lambda vector (all zeros, i.e. at the origin).
    std::vector<double> lambda(nConstraints + 1);
    std::fill(lambda.begin(), lambda.end(), 0.0);

    // Initialise gradient vector.
    std::vector<double> gradient(nConstraints + 1);

    // Loop over all functions: objective first, then constraints.
    for (unsigned int i=0;i<nConstraints+1;i++)
    {
        // Calculate the gradient.
        //HAC std::cout << "start of computeGradient" << "\n";
        
        computeGradients(lambda, gradient, i);
        //HAC std::cout << "end of computeGradient" << "\n";
        // Scale by diagonal gradient entry (absolute value).
        scaleFactors[i] *= (1.0 / std::abs(gradient[i]));
    }
}

void Optimise::computeConstraintDistances()
{
    /* If we are far from satisfying the constraint then we need
       to scale the constraint distance so that it can be "satisfied"
       by simply moving in the correct direction, i.e. moving towards
       satisying the constraint.

       If we are well within the region where an inequality constraint
       is satisfied, then the constraint can be removed from the optimisation
       problem. Here we create a map between indices for the vector of active
       constraints and the original constraints
       vector.
     */

    // Initialise the index map.
    indexMap.clear();
    indexMap.push_back(0);

    // Number of active contraints.
    unsigned int nActive = 0;

    // Loop over all constraints.
    for (unsigned int i=0;i<nConstraints;i++)
    {
        // Flag constraint as active.
        bool isActive = true;

        // Min and max constraint changes.
        double min = 0;
        double max = 0;

        // Integrate over boundary points.
        for (unsigned int j=0;j<nPoints;j++)
        {
            // Don't consider fixed points.
            if (!boundaryPoints[j].isFixed)
            {
                if (boundaryPoints[j].sensitivities[i+1] > 0)
                {
                    min += boundaryPoints[j].sensitivities[i+1]
                        * boundaryPoints[j].length
                        * boundaryPoints[j].negativeLimit;

                    max += boundaryPoints[j].sensitivities[i+1]
                        * boundaryPoints[j].length
                        * boundaryPoints[j].positiveLimit;
                }
                else
                {
                    min += boundaryPoints[j].sensitivities[i+1]
                        * boundaryPoints[j].length
                        * boundaryPoints[j].positiveLimit;

                    max += boundaryPoints[j].sensitivities[i+1]
                        * boundaryPoints[j].length
                        * boundaryPoints[j].negativeLimit;
                }
            }
        }

        // Scale (20% is arbitrary, but seems to work well).
        min *= 0.2;
        max *= 0.2;

        // Constraint is violated.
        if (constraintDistances[i] < 0)
        {
            if (constraintDistances[i] < min)
                constraintDistancesScaled[i] = min;
        }

        // Constraint is satisfied.
        else
        {
            if (constraintDistances[i] > max)
            {
                // Flag inequality constraint as inactive.
                if (!isEquality[i]) isActive = false;

                else constraintDistancesScaled[i] = max;
            }
        }

        // Constraint is active.
        if (isActive)
        {
            // Shift initial lambda estimate.
            lambdas[nActive+1] = lambdas[i+1];

            // Shift constraint distance.
            constraintDistancesScaled[nActive] = constraintDistancesScaled[i];

            // Shift equality flag.
            isEquality[nActive] = isEquality[i];

            // Map the constraint index: active --> original
            indexMap.push_back(i+1); // FIXTHIS: TAG_indexmap

            // Incremement the number of active constraints.
            nActive++;
        }
    }

    // Resize vectors if constraints have been removed.
    if (nActive < nConstraints)
    {
        lambdas.resize(nActive + 1);
        negativeLambdaLimits.resize(nActive + 1);
        positiveLambdaLimits.resize(nActive + 1);
        scaleFactors.resize(nActive + 1);
        constraintDistancesScaled.resize(nActive);
        isEquality.resize(nActive);

        // Store the original number of constraints.
        nConstraintsInitial = nConstraints;

        // Reduce the number of constraints.
        nConstraints = nActive;
    }
    // std::cout << "computeContstraints = " << constraintDistances[0] << "and scaled:" <<
    // constraintDistancesScaled[0] << "\n";
}

void Optimise::computeLambdaLimits() // FIXTHIS: TAG_lambdas
{
    /* The lambda limits are set by computing the minimum displacement
    that violates the CFL condition independently for each function, i.e.
    when setting other lambda values equal to zero.

    In this case the displacement for a given function is simply

        z = lambda x sensitivity

    and the largest lambda that doesn't trigger the CFL limit is

        lambda = CFL / max(abs(sensitivity))
    */

    // Loop over objective and constraints.
    for (unsigned int i=0;i<nConstraints+1;i++)
    {
        // Remap the sensitivity index: active --> original
        unsigned int k = indexMap[i];

        // Initialise limits.
        negativeLambdaLimits[i] = positiveLambdaLimits[i] = 0;

        // Loop over all boundary points.
        for (unsigned int j=0;j<boundaryPoints.size();j++)
        {
            // Don't consider fixed points.
            if (!boundaryPoints[j].isFixed)
            {
                // Store sensitivity.
                double sens = boundaryPoints[j].sensitivities[k];

                // Initialise min and max displacements.
                double minDisp, maxDisp;

                // Set limits depending on sign of the sensitivity.
                if (sens > 0)
                {
                    minDisp = boundaryPoints[j].negativeLimit / sens;
                    maxDisp = boundaryPoints[j].positiveLimit / sens;
                }
                else
                {
                    maxDisp = boundaryPoints[j].negativeLimit / sens;
                    minDisp = boundaryPoints[j].positiveLimit / sens;
                }

                // Update limits.

                if (maxDisp > positiveLambdaLimits[i])
                    positiveLambdaLimits[i] = maxDisp;

                if (minDisp < negativeLambdaLimits[i])
                    negativeLambdaLimits[i] = minDisp;
            }
        }

        // Scale limits.
        negativeLambdaLimits[i] /= scaleFactors[i];
        positiveLambdaLimits[i] /= scaleFactors[i];

        /* Rescale the lambda values so that they are in range.

            N.B. Resetting the lambda values to zero can cause spurious
            errors with the optimiser when close to convergence. Solutions
            may be found with lambda = 0 (for the objective) which will
            result in undefined velocities.
        */
        while (lambdas[i] < negativeLambdaLimits[i]) lambdas[i] *= 0.9;
        while (lambdas[i] > positiveLambdaLimits[i]) lambdas[i] *= 0.9;
    }

    /* Check for a zero negative lambda limit for the objective function.
       This can occur for compliance minimisation when the initial domain
       is completely filled.
     */
    if (negativeLambdaLimits[0] == 0) negativeLambdaLimits[0] = -1e-2;
}

void Optimise::computeDisplacements(const std::vector<double>& lambda)
{
    // Loop over all boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
        {
            // Reset side limit flag.
            isSideLimit[i] = false;

            // Initialise component for objective.
            displacements[i] = scaleFactors[0] * lambda[0] * boundaryPoints[i].sensitivities[0];
            
            // std::cout << "(" << i << "): " << displacements[i] << "\n";
            // Add components for active constraints.
            for (unsigned int j=1;j<nConstraints+1;j++)
            {
                // Remap the sensitivity index: active --> original
                unsigned int k = indexMap[j];

                // Update displacement vector.
                displacements[i] += scaleFactors[j] * lambda[j] * boundaryPoints[i].sensitivities[k];
            }

            // Check side limits if point lies close to domain boundary.
            if (boundaryPoints[i].isDomain)
            {
                // Apply side limit.
                if (displacements[i] < boundaryPoints[i].negativeLimit)
                {
                    displacements[i] = boundaryPoints[i].negativeLimit;
                    isSideLimit[i] = true;
                }
            }

            // If capping displacements is selected.
            if (isCapped)
            {
                // Capping displacements when they violate the CFL condition.
                if (displacements[i] > maxDisplacement)
                {
                    displacements[i] = maxDisplacement;
                }
            }   
        }
    }
}

void Optimise::computePartialDisplacement(std::vector<int>& rows, std::vector<int>& cols, 
    std::vector<double>& data, const std::vector<double>& lambda)
{
    rows.resize(nPoints * (nConstraints+1), 0);
    cols.resize(nPoints * (nConstraints+1), 0);
    data.resize(nPoints * (nConstraints+1), 0.0);

    int cnt = 0; 
    // Loop over all boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        rows[cnt] = i;
        cols[cnt] = 0;
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
        {
            int cnt_cols = 0;            
            // Reset side limit flag.
            isSideLimit[i] = false;

            // Initialise component for objective.
            data[cnt] = scaleFactors[0] * boundaryPoints[i].sensitivities[0];
            
            // std::cout << "(" << i << "): " << displacements[i] << "\n";
            // Add components for active constraints.
            for (unsigned int j=1;j<nConstraints+1;j++)
            {
                // Remap the sensitivity index: active --> original
                unsigned int k = indexMap[j];

                // Update displacement vector.
                rows[cnt+j] = i;
                cols[cnt+j] = j;
                data[cnt+j] = scaleFactors[j] * boundaryPoints[i].sensitivities[k];
            }

            // Check side limits if point lies close to domain boundary.
            if (boundaryPoints[i].isDomain)
            {
                // Apply side limit.
                if (displacements[i] < boundaryPoints[i].negativeLimit)
                {
                    // displacements[i] = boundaryPoints[i].negativeLimit;
                    data[cnt] = 0.0;
                    for (unsigned int j=1;j<nConstraints+1;j++){
                        data[cnt+j] = 0.0;
                    }
                    isSideLimit[i] = true;
                }
            }

            // If capping displacements is selected.
            if (isCapped)
            {
                // Capping displacements when they violate the CFL condition.
                if (displacements[i] > maxDisplacement)
                {   
                    data[cnt] = 0.0;
                    for (unsigned int j=1;j<nConstraints+1;j++){
                        data[cnt+j] = 0.0;
                    }
                    // displacements[i] = maxDisplacement;
                }
            } 
            // return partialD;  
            cnt += 1+nConstraints;
            
        }
    }
}


// // HAC: added for gradient computation /// WIP
// void Optimise::computeGradDisplacements(std::vector<double>& data, std::vector<int>& row, std::vector<int>& col)
// {
//     // reserve size
//     data.resize(nPoints * (nConstraints+1), 0.0);
//     row.resize(nPoints * (nConstraints+1), 0);
//     col.resize(nPoints * (nConstraints+1), 0);

//     // Loop over all boundary points.
//     for (unsigned int i=0;i<nPoints;i++)
//     {
//         // Don't consider fixed points.
//         if (!boundaryPoints[i].isFixed)
//         {
//             // Reset side limit flag.
//             isSideLimit[i] = false;

//             // Initialise component for objective.
//             displacements[i] = scaleFactors[0] * lambda[0] * boundaryPoints[i].sensitivities[0];

//             // Add components for active constraints.
//             for (unsigned int j=1;j<nConstraints+1;j++)
//             {
//                 // Remap the sensitivity index: active --> original
//                 unsigned int k = indexMap[j];

//                 // Update displacement vector.
//                 displacements[i] += scaleFactors[j] * lambda[j] * boundaryPoints[i].sensitivities[k];
//             }

//             // Check side limits if point lies close to domain boundary.
//             if (boundaryPoints[i].isDomain)
//             {
//                 // Apply side limit.
//                 if (displacements[i] < boundaryPoints[i].negativeLimit)
//                 {
//                     displacements[i] = boundaryPoints[i].negativeLimit;
//                     isSideLimit[i] = true;
//                 }
//             }

//             // If capping displacements is selected.
//             if (isCapped)
//             {
//                 // Capping displacements when they violate the CFL condition.
//                 if (displacements[i] > maxDisplacement)
//                 {
//                     displacements[i] = maxDisplacement;
//                 }
//             }   
//         }
//     }
// }


double Optimise::computeFunction(unsigned int index)
{
    // This method assumes that displacements have already been calculated.

    // Remap the sensitivity index: active --> original
    unsigned int j = indexMap[index];

    // Initialise function.
    double func = 0;

    // Integrate function over boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
            func += (scaleFactors[index] * displacements[i] * boundaryPoints[i].sensitivities[j] * boundaryPoints[i].length);
    }

    if (index == 0) return func;
    else return (func - (scaleFactors[index] * constraintDistancesScaled[index - 1]));
}

double Optimise::computeFunction(std::vector<double>& displacement_cython, unsigned int index)
{
    // This method assumes that displacements have already been calculated.

    // Remap the sensitivity index: active --> original
    unsigned int j = indexMap[index];

    // Initialise function.
    double func = 0;

    // Integrate function over boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
            func += (scaleFactors[index] * displacement_cython[i] * boundaryPoints[i].sensitivities[j] * boundaryPoints[i].length);
    }

    if (index == 0) return func;
    else return (func - (scaleFactors[index] * constraintDistancesScaled[index - 1]));
}

std::vector<double>  Optimise::computePartialFunction(unsigned int index)
{
    // This method assumes that displacements have already been calculated.

    // Remap the sensitivity index: active --> original
    unsigned int j = indexMap[index];

    // Initialise function.
    std::vector<double> func(nPoints, 0.0);

    // Integrate function over boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
            func[i] += (scaleFactors[index] * boundaryPoints[i].sensitivities[j] * boundaryPoints[i].length);
    }

    return func;
}

void Optimise::computeGradients(const std::vector<double>& lambda, std::vector<double>& gradient, unsigned int index)
{
    // Whether we're at the origin, i.e. all lambda values are zero.
    bool isOrigin = false;

    // Zero the gradients.
    gradient[0] = 0;
    for (unsigned int i=1;i<nConstraints+1;i++) gradient[i] = 0;

    /* If all lambda values are zero then we need a fix for the
       analytic gradient calculation. This is because side constraints
       for boundary points lying exactly on the domain boundary will
       be active for lambda < 0 and inactive for lambda > 0. As such,
       the gradient will be half as large, i.e. only positive lambda
       contributes.
     */

    // Sum lambda values.
    double lambdaSum = 0;
    for (unsigned int i=0;i<nConstraints+1;i++) lambdaSum += std::abs(lambda[i]);

    // If all lambdas are zero, then we're at the origin.
    if (lambdaSum < 1e-6)
    {
        isOrigin = true;

        // Find points lying on domain boundary.
        for (unsigned int i=0;i<nPoints;i++)
        {
            if (boundaryPoints[i].isDomain) isSideLimit[i] = true;
            else isSideLimit[i] = false;
        }
    }

    // Calculate the derivative with respect to each lambda.
    //HAC std::cout << "start of derivative for each lambda" << "\n";
    // Loop over all points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
        {
            // Loop over all functions (objective, then constraints).
            for (unsigned int j=0;j<nConstraints+1;j++)
            {
                // Remap the sensitivity index: active --> original
                unsigned int k = indexMap[j];

                // Scale factor.
                double scaleFactor = scaleFactors[index] * scaleFactors[k];

                if (!isSideLimit[i])
                {
                    gradient[k] += (boundaryPoints[i].sensitivities[index]
                                * boundaryPoints[i].sensitivities[k]
                                * boundaryPoints[i].length
                                * scaleFactor);
                }
                else if (isOrigin)
                {
                    gradient[k] += (boundaryPoints[i].sensitivities[index]
                                * boundaryPoints[i].sensitivities[k]
                                * boundaryPoints[i].length
                                * 0.5 * scaleFactor);
                }
            }
        }
    }
}

double Optimise::rescaleDisplacements(std::vector<double>& lambdas)
{
    // Check for CFL violation and rescale the displacments
    // and lambda values if necessary.

    // Maximum displacement magnitude.
    double maxDisp = 0;

    // Displacment scale factor.
    double scale;

    // Loop over all boundary points.
    for (unsigned int i=0;i<nPoints;i++)
    {
        // Don't consider fixed points.
        if (!boundaryPoints[i].isFixed)
        {
            // Absolute displacement.
            double disp = std::abs(displacements[i]);

            // Displacement exceeds the CFL limit.
            if (disp > maxDisplacement)
            {
                // Check if current maximum is exceeded.
                if (disp > maxDisp)
                {
                    // Store maximum displacement and scaling factor.
                    maxDisp = disp;
                    scale = maxDisplacement / maxDisp;
                }
            }
        }
    }
    
    // CFL condition is violated, rescale displacements.
    if (maxDisp)
    {
        // Scale lambda values.
        for (unsigned int i=0;i<nConstraints+1;i++)
            lambdas[i] *= scale;

        // Recompute the displacement vector.
        computeDisplacements(lambdas); // FIXTHIS: TAG_lambdas

        // Return the scale factor.
        return scale;
    }

    return 1.0;
}

void Optimise::LaplacianDisplacementsFilter(int iterations)
{
    // Neighbours variables.
    int neig0, neig1;

    // Vector of filtered sensitivities.
    std::vector<double> filteredDisp_n0;
    filteredDisp_n0.resize(nPoints);
    std::vector<double> filteredDisp_n1;
    filteredDisp_n1.resize(nPoints);

    // Initialising filtered displacements.
    for (int i = 0; i < nPoints; i++)
    {
        filteredDisp_n0[i] = displacements[i];
    }

    // Laplacian smoothing iteration.
    for (int i = 0; i < iterations; i++)
    {
        // Loop over all points.
        for (int i = 0; i < nPoints; i++)
        {
            if (boundaryPoints[i].nNeighbours == 2)
            {
                // Getting neighbour points.
                neig0 = boundaryPoints[i].neighbours[0];
                neig1 = boundaryPoints[i].neighbours[1];

                // Applying Laplacian smoothing.
                filteredDisp_n1[i] = filteredDisp_n0[i] + 0.5*(0.5*(filteredDisp_n0[neig0]-filteredDisp_n0[i])+0.5*(filteredDisp_n0[neig1]-filteredDisp_n0[i]));
            }
            else if (boundaryPoints[i].nNeighbours == 1)
            {
                // Getting neighbour points.
                neig0 = boundaryPoints[i].neighbours[0];

                // Applying Laplacian smoothing.
                filteredDisp_n1[i] = filteredDisp_n0[i] + 0.5*(filteredDisp_n0[neig0]-filteredDisp_n0[i]);
            }
            else
            {
                std::cout << "Point does not have 2 neither 1 neighbouring points!" << "\n";
            }
        }

        // Updating n+1 filtered sensitivities.
        for (int i = 0; i < nPoints; i++)
        {
            displacements[i] = filteredDisp_n1[i];
            filteredDisp_n0[i] = filteredDisp_n1[i];
            filteredDisp_n1[i] = 0.0;
        }
    }

    // Computing maximum displacement magnitude and new scale.
    double maxDisp = 0;
    double scale = 0;
    for (int i = 0; i < nPoints; i++)
    {
        // Absolute displacement.
        double disp = std::abs(displacements[i]);

        // Check if current maximum is exceeded.
        if (disp > maxDisp)
        {
            // Store maximum displacement and scaling factor.
            maxDisp = disp;
            scale = maxDisplacement / maxDisp;
        }
    }
    for (int i = 0; i < nPoints; i++)
    {
        displacements[i] *= scale;
    }

}


