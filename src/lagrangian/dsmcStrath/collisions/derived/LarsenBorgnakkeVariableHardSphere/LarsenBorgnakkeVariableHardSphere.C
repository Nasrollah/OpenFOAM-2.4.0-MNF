/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2009-2010 OpenCFD Ltd.
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "LarsenBorgnakkeVariableHardSphere.H"
#include "constants.H"
#include "addToRunTimeSelectionTable.H"

using namespace Foam::constant::mathematical;

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(LarsenBorgnakkeVariableHardSphere, 0);
    addToRunTimeSelectionTable(BinaryCollisionModel, LarsenBorgnakkeVariableHardSphere, dictionary);
};

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

//template <class CloudType>
Foam::LarsenBorgnakkeVariableHardSphere::LarsenBorgnakkeVariableHardSphere
(
    const dictionary& dict,
    dsmcCloud& cloud
)
:
    BinaryCollisionModel(dict, cloud),
    coeffDict_(dict.subDict(typeName + "Coeffs")),
    Tref_(readScalar(coeffDict_.lookup("Tref"))),
    rotationalRelaxationCollisionNumber_
    (
        readScalar(coeffDict_.lookup("rotationalRelaxationCollisionNumber"))
    ),
    electronicRelaxationCollisionNumber_
    (
        readScalar(coeffDict_.lookup("electronicRelaxationCollisionNumber"))
    )
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

//template <class CloudType>
Foam::LarsenBorgnakkeVariableHardSphere::~LarsenBorgnakkeVariableHardSphere()
{}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::LarsenBorgnakkeVariableHardSphere::active() const
{
    return true;
}

//template <class CloudType>
Foam::scalar Foam::LarsenBorgnakkeVariableHardSphere::sigmaTcR
(
    const dsmcParcel& pP,
    const dsmcParcel& pQ
) const
{
    //const CloudType& cloud(this->owner());
    
    label typeIdP = pP.typeId();
    label typeIdQ = pQ.typeId();

    scalar dPQ =
        0.5
       *(
            cloud_.constProps(typeIdP).d()
          + cloud_.constProps(typeIdQ).d()
        );

    scalar omegaPQ =
        0.5
       *(
            cloud_.constProps(typeIdP).omega()
          + cloud_.constProps(typeIdQ).omega()
        );

    scalar cR = mag(pP.U() - pQ.U());

    if (cR < VSMALL)
    {
        return 0;
    }

    scalar mP = cloud_.constProps(typeIdP).mass();

    scalar mQ = cloud_.constProps(typeIdQ).mass();

    scalar mR = mP*mQ/(mP + mQ);

    // calculating cross section = pi*dPQ^2, where dPQ is from Bird, eq. 4.79
    scalar sigmaTPQ =
        pi*dPQ*dPQ
       *pow(2.0*physicoChemical::k.value()*Tref_/(mR*cR*cR), omegaPQ - 0.5)
       /exp(Foam::lgamma(2.5 - omegaPQ));

    return sigmaTPQ*cR;
}


//template <class CloudType>
void Foam::LarsenBorgnakkeVariableHardSphere::collide
(
    dsmcParcel& pP,
    dsmcParcel& pQ
)
{   
    label typeIdP = pP.typeId();
    label typeIdQ = pQ.typeId();
    vector& UP = pP.U();
    vector& UQ = pQ.U();
    scalar& ERotP = pP.ERot();
    scalar& ERotQ = pQ.ERot();
    scalar EVibP = pP.vibLevel()*cloud_.constProps(typeIdP).thetaV()*physicoChemical::k.value();
    scalar EVibQ = pQ.vibLevel()*cloud_.constProps(typeIdQ).thetaV()*physicoChemical::k.value();
    label& vibLevelP = pP.vibLevel();
    label& vibLevelQ = pQ.vibLevel();
    label& ELevelP = pP.ELevel();
    label& ELevelQ = pQ.ELevel();

    Random& rndGen(cloud_.rndGen());
    
    
  //   VIBRATIONAL ENERGY EXCHANGE - QUANTUM-KINETIC MODEL
    
    scalar preCollisionERotP = ERotP;
    scalar preCollisionERotQ = ERotQ;
    
    scalar preCollisionEVibP = EVibP;
    scalar preCollisionEVibQ = EVibQ;
    
    scalar preCollisionEEleP = cloud_.constProps(typeIdP).electronicEnergyList()[ELevelP];
    scalar preCollisionEEleQ = cloud_.constProps(typeIdQ).electronicEnergyList()[ELevelQ];

    scalar rotationalDofP = cloud_.constProps(typeIdP).rotationalDegreesOfFreedom();
    scalar rotationalDofQ = cloud_.constProps(typeIdQ).rotationalDegreesOfFreedom();
    
    scalar vibrationalDofP = cloud_.constProps(typeIdP).vibrationalDegreesOfFreedom();
    scalar vibrationalDofQ = cloud_.constProps(typeIdQ).vibrationalDegreesOfFreedom();
    
    label jMaxP = cloud_.constProps(typeIdP).numberOfElectronicLevels();    
    label jMaxQ = cloud_.constProps(typeIdQ).numberOfElectronicLevels();
    
    List<scalar> EElistP = cloud_.constProps(typeIdP).electronicEnergyList();    
    List<scalar> EElistQ = cloud_.constProps(typeIdQ).electronicEnergyList();
   
    List<label> gListP = cloud_.constProps(typeIdP).degeneracyList();    
    List<label> gListQ = cloud_.constProps(typeIdQ).degeneracyList();  
    
    scalar thetaVP = cloud_.constProps(typeIdP).thetaV();  
    scalar thetaVQ = cloud_.constProps(typeIdQ).thetaV();
    
    scalar thetaDP = cloud_.constProps(typeIdP).thetaD();
    scalar thetaDQ = cloud_.constProps(typeIdQ).thetaD();
    
    scalar ZrefP = cloud_.constProps(typeIdP).Zref();
    scalar ZrefQ = cloud_.constProps(typeIdQ).Zref();
    
    scalar refTempZvP = cloud_.constProps(typeIdP).TrefZv();
    scalar refTempZvQ = cloud_.constProps(typeIdQ).TrefZv();

    scalar omegaPQ =
        0.5
       *(
            cloud_.constProps(typeIdP).omega()
          + cloud_.constProps(typeIdQ).omega()
        );

    scalar mP = cloud_.constProps(typeIdP).mass();
    scalar mQ = cloud_.constProps(typeIdQ).mass();

    scalar mR = mP*mQ/(mP + mQ);
    vector Ucm = (mP*UP + mQ*UQ)/(mP + mQ);
    scalar cRsqr = magSqr(UP - UQ);

    scalar translationalEnergy = 0.5*mR*cRsqr;
                
    scalar ChiB = 2.5 - omegaPQ;
    
    scalar inverseRotationalCollisionNumber = 1.0/rotationalRelaxationCollisionNumber_;
    scalar inverseElectronicCollisionNumber = 1.0/electronicRelaxationCollisionNumber_;
    
//     scalar alphaPQ = 2.0/(omegaPQ-0.5);
    
//     scalar zeta_T = 4.0 - (4.0/alphaPQ);
    
//     Info << "alphaPQ = " << alphaPQ << endl;
//     Info << "zeta_T = " << zeta_T << endl;
            
    // Larsen Borgnakke rotational energy redistribution part.  Using the serial
    // application of the LB method, as per the INELRS subroutine in Bird's
    // DSMC0R.FOR
    
    if(inverseElectronicCollisionNumber > rndGen.scalar01())
    { 
//         if( jMaxP > 1) // Only neutrals and ions have more than 1 electronic energy level
//         {
            // collision energy of particle P = relative translational energy + pre-collision electronic energy
            
            scalar EcP = translationalEnergy + preCollisionEEleP;
            
            label postCollisionELevel = cloud_.postCollisionElectronicEnergyLevel
                            (
                                EcP,
                                jMaxP,
                                omegaPQ,
                                EElistP,
                                gListP
                            );
                            
            ELevelP = postCollisionELevel;
            
            // relative translational energy after electronic exchange
            translationalEnergy = EcP - EElistP[ELevelP];
//         }
    }
            
    if(vibrationalDofP > VSMALL)
    {
        // collision energy of particle P = relative translational energy + pre-collision vibrational energy
        scalar EcP = translationalEnergy + preCollisionEVibP; 

        // - maximum possible quantum level (equation 3, Bird 2010)
        label iMaxP = (EcP / (physicoChemical::k.value()*thetaVP)); 

        if(iMaxP > SMALL)
        {       
            vibLevelP = cloud_.postCollisionVibrationalEnergyLevel
                    (
                        false,
                        vibLevelP,
                        iMaxP,
                        thetaVP,
                        thetaDP,
                        refTempZvP,
                        omegaPQ,
                        ZrefP,
                        EcP
                     );
                    
            translationalEnergy = EcP - (vibLevelP*cloud_.constProps(typeIdP).thetaV()*physicoChemical::k.value());
        }
    }
    
    if (rotationalDofP > VSMALL)
    {
//         scalar particleProbabilityP = 
//             ((zeta_T + 2.0*rotationalDofP)/(2.0*rotationalDofP))
//             *(
//                 1.0 - sqrt(
//                             1.0 - (rotationalDofP/zeta_T)
//                             *((zeta_T+rotationalDofP)/(zeta_T+2.0*rotationalDofP))
//                             *(4.0/rotationalRelaxationCollisionNumber_)
//                           )
//              );
            
//         Info << "particleProbabilityP = " << particleProbabilityP << endl;
        
        if (inverseRotationalCollisionNumber /*particleProbabilityP*/ > rndGen.scalar01())
        {
            scalar EcP = translationalEnergy + preCollisionERotP;
            
            scalar energyRatio = cloud_.postCollisionRotationalEnergy(rotationalDofP,ChiB);

            ERotP = energyRatio*EcP;
        
            translationalEnergy = EcP - ERotP;
        }
    }   
    
    if(inverseElectronicCollisionNumber > rndGen.scalar01())
    {
//         if( jMaxQ > 1) // Only neutrals and ions have more than 1 electronic energy level
//         {
            // collision energy of particle Q = relative translational energy + pre-collision electronic energy
            
            scalar EcQ = translationalEnergy + preCollisionEEleQ; 
            
            label postCollisionELevel = cloud_.postCollisionElectronicEnergyLevel
                            (
                                EcQ,
                                jMaxQ,
                                omegaPQ,
                                EElistQ,
                                gListQ
                            );
                            
            ELevelQ = postCollisionELevel;

            // relative translational energy after electronic exchange
            translationalEnergy = EcQ - EElistQ[ELevelQ];
//         }
    }
              
    if(vibrationalDofQ > VSMALL)
    {
        
         // collision energy of particle Q = relative translational energy + pre-collision vibrational energy
        scalar EcQ = translationalEnergy + preCollisionEVibQ; 

        // - maximum possible quantum level (equation 3, Bird 2010)
        label iMaxQ = (EcQ / (physicoChemical::k.value()*thetaVQ)); 

        if(iMaxQ > SMALL)
        {       
            vibLevelQ = cloud_.postCollisionVibrationalEnergyLevel
                    (
                        false,
                        vibLevelQ,
                        iMaxQ,
                        thetaVQ,
                        thetaDQ,
                        refTempZvQ,
                        omegaPQ,
                        ZrefQ,
                        EcQ
                     );
                    
            translationalEnergy = EcQ - (vibLevelQ*cloud_.constProps(typeIdQ).thetaV()*physicoChemical::k.value());
        }
    }
        
    if (rotationalDofQ > VSMALL)
    {
//         scalar particleProbabilityQ = 
//             ((zeta_T + 2.0*rotationalDofQ)/(2.0*rotationalDofQ))
//             *(
//                 1.0 - sqrt(
//                             1.0 - (rotationalDofQ/zeta_T)
//                             *((zeta_T+rotationalDofQ)/(zeta_T+2.0*rotationalDofQ))
//                             *(4.0/rotationalRelaxationCollisionNumber_)
//                           )
//              );
            
//         Info << "particleProbabilityQ = " << particleProbabilityQ << endl;
        
        if (inverseRotationalCollisionNumber /*particleProbabilityQ*/ > rndGen.scalar01())
        {
            scalar EcQ = translationalEnergy + preCollisionERotQ;
            
            scalar energyRatio = cloud_.postCollisionRotationalEnergy(rotationalDofQ,ChiB);

            ERotQ = energyRatio*EcQ;
        
            translationalEnergy = EcQ - ERotQ;
        }
    }

    // Rescale the translational energy
    scalar cR = sqrt((2.0*translationalEnergy)/mR);

    // Variable Hard Sphere collision part

    scalar cosTheta = 2.0*rndGen.scalar01() - 1.0;

    scalar sinTheta = sqrt(1.0 - cosTheta*cosTheta);

    scalar phi = 2.0*pi*rndGen.scalar01();

    vector postCollisionRelU =
        cR
       *vector
        (
            cosTheta,
            sinTheta*cos(phi),
            sinTheta*sin(phi)
        );

    UP = Ucm + postCollisionRelU*mQ/(mP + mQ);

    UQ = Ucm - postCollisionRelU*mP/(mP + mQ);
}

const Foam::dictionary& Foam::LarsenBorgnakkeVariableHardSphere::coeffDict() const
{
    return coeffDict_;
}


// ************************************************************************* //
