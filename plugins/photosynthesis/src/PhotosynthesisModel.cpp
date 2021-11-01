/** \file "PhotosynthesisModel.cpp" Primary source file for photosynthesis plug-in.
    \author Brian Bailey

    Copyright (C) 2018  Brian Bailey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "PhotosynthesisModel.h"

using namespace std;
using namespace helios;

PhotosynthesisModel::PhotosynthesisModel( helios::Context* a_context ){
    context = a_context;

    //default values set here
    model_flag = 1; //empirical model

    i_PAR_default = 0;
    TL_default = 300;
    CO2_default = 390;
    gM_default = 1;


}

int PhotosynthesisModel::selfTest(){

    std::cout << "Running photosynthesis model self-test..." << std::flush;

    Context context_test;

    float errtol = 0.001f;

    uint UUID = context_test.addPatch( make_vec3(0,0,0), make_vec2(1,1) );

    PhotosynthesisModel photomodel(&context_test);

    std::vector<float> A;

    float Qin[9] = {0, 50, 100, 200, 400, 800, 1200, 1500, 2000};
    A.resize(9);
    std::vector<float> AQ_expected{-2.3948,8.7823,13.7131,18.2686,21.5700,21.5700,21.5700,21.5700,21.5700};

    //Generate a light response curve using empirical model with default parmeters
    for( int i=0; i<9; i++ ){
        context_test.setPrimitiveData(UUID,"radiation_flux_PAR",Qin[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
    }

    //Generate a light response curve using Farquhar model with default parameters

    photomodel.setModelType_Farquhar();

    for( int i=0; i<9; i++ ){
        context_test.setPrimitiveData(UUID,"radiation_flux_PAR",Qin[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
        if( fabs(A.at(i)-AQ_expected.at(i))/fabs(AQ_expected.at(i))>errtol ){
            std::cout << "failed. Incorrect light response curve." << std::endl;
            return 1;
        }
    }

    //Generate an A vs Ci curve using empirical model with default parameters

    float CO2[9] = {100, 200, 300, 400, 500, 600, 700, 800, 1000};
    A.resize(9);

    context_test.setPrimitiveData(UUID,"radiation_flux_PAR",Qin[8]);

    photomodel.setModelType_Empirical();
    for( int i=0; i<9; i++ ){
        context_test.setPrimitiveData(UUID,"air_CO2",CO2[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
    }

    //Generate an A vs Ci curve using Farquhar model with default parameters

    std::vector<float> ACi_expected{2.4503,10.0469,16.5228,22.0911,26.9183,29.2785,30.4181,31.2971,32.5621};

    photomodel.setModelType_Farquhar();
    for( int i=0; i<9; i++ ){
        context_test.setPrimitiveData(UUID,"air_CO2",CO2[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
        if( fabs(A.at(i)-ACi_expected.at(i))/fabs(ACi_expected.at(i))>errtol ){
            std::cout << "failed. Incorrect CO2 response curve." << std::endl;
            return 1;
        }
    }

    //Generate an A vs temperature curve using empirical model with default parameters

    float TL[7] = {270, 280, 290, 300, 310, 320, 330};
    A.resize(7);

    context_test.setPrimitiveData(UUID,"air_CO2",CO2[3]);

    photomodel.setModelType_Empirical();
    for( int i=0; i<7; i++ ){
        context_test.setPrimitiveData(UUID,"temperature",TL[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
    }

    //Generate an A vs temperature curve using Farquhar model with default parameters

    std::vector<float> AT_expected{3.8978,9.1137,16.4810,22.0911,22.2232,16.4393,5.5393};

    photomodel.setModelType_Farquhar();
    for( int i=0; i<7; i++ ){
        context_test.setPrimitiveData(UUID,"temperature",TL[i]);
        photomodel.run();
        context_test.getPrimitiveData(UUID,"net_photosynthesis",A[i]);
        if( fabs(A.at(i)-AT_expected.at(i))/fabs(AT_expected.at(i))>errtol ){
            std::cout << "failed. Incorrect temperature response curve." << std::endl;
            return 1;
        }
    }

    std::cout << "passed." << std::endl;

    return 0;
}

void PhotosynthesisModel::setModelType_Empirical(){
    model_flag=1;
}

void PhotosynthesisModel::setModelType_Farquhar(){
    model_flag=2;
}

void PhotosynthesisModel::setModelCoefficients(const EmpiricalModelCoefficients &modelcoefficients ){
    empiricalmodelcoeffs = modelcoefficients;
}

void PhotosynthesisModel::setModelCoefficients(const FarquharModelCoefficients &modelcoefficients ){
    farquharmodelcoeffs = modelcoefficients;
}

void PhotosynthesisModel::run(){
    run(context->getAllUUIDs());
}

void PhotosynthesisModel::run(const std::vector<uint> &lUUIDs ){

    for( uint UUID : lUUIDs){

        float i_PAR;
        if( context->doesPrimitiveDataExist(UUID,"radiation_flux_PAR") ){
            context->getPrimitiveData(UUID,"radiation_flux_PAR",i_PAR);
            i_PAR = i_PAR*4.57f; //umol/m^2-s (ref https://www.controlledenvironments.org/wp-content/uploads/sites/6/2017/06/Ch01.pdf)
            if( i_PAR<0 ){
                i_PAR = 0;
                std::cout << "WARNING (runPhotosynthesis): PAR flux value provided was negative.  Clipping to zero." << std::endl;
            }
        }else{
            i_PAR = i_PAR_default;
        }

        float TL;
        if( context->doesPrimitiveDataExist(UUID,"temperature") ){
            context->getPrimitiveData(UUID,"temperature",TL);
            if( TL<0 ){
                TL = 0;
                std::cout << "WARNING (runPhotosynthesis): Temperature value provided was negative. Clipping to zero. Are you using absolute temperature units?" << std::endl;
            }
        }else{
            TL = TL_default;
        }

        float CO2;
        if( context->doesPrimitiveDataExist(UUID,"air_CO2") ){
            context->getPrimitiveData(UUID,"air_CO2",CO2);
            if( CO2<0 ){
                CO2 = 0;
                std::cout << "WARNING (runPhotosynthesis): CO2 concentration value provided was negative. Clipping to zero." << std::endl;
            }
        }else{
            CO2 = CO2_default;
        }

        float gM;
        if( context->doesPrimitiveDataExist(UUID,"moisture_conductance") ){
            context->getPrimitiveData(UUID,"moisture_conductance",gM);
            if( gM<0 ){
                gM = 0;
                std::cout << "WARNING (runPhotosynthesis): Moisture conductance value provided was negative. Clipping to zero." << std::endl;
            }
        }else{
            gM = gM_default;
        }

        float A, Ci;
        int limitation_state;

        if( model_flag==2 ){ //Farquhar-von Caemmerer-Berry Model
            A = evaluateFarquharModel( i_PAR, TL, CO2, gM, Ci, limitation_state );
        }else{ //Empirical Model
            A = evaluateEmpiricalModel( i_PAR, TL, CO2, gM );
        }

        if( A==0 ){
            std::cout << "WARNING (PhotosynthesisModel): Solution did not converge for primitive " << UUID << "." << std::endl;
        }

        context->setPrimitiveData(UUID,"net_photosynthesis",HELIOS_TYPE_FLOAT,1,&A);

        for( const auto &data : output_prim_data ){
            if( data=="Ci" && model_flag==2 ){
                context->setPrimitiveData(UUID,"Ci",Ci);
            }else if( data=="limitation_state" && model_flag==2 ){
                context->setPrimitiveData(UUID,"limitation_state",limitation_state);
            }
        }

    }


}

float PhotosynthesisModel::evaluateCi_Empirical( float Ci, float CO2, float fL, float Rd, float gM ) const{


    //--- CO2 Response Function --- //

    float fC = empiricalmodelcoeffs.kC*Ci/empiricalmodelcoeffs.Ci_ref;


    //--- Assimilation Rate --- //

    float A = empiricalmodelcoeffs.Asat*fL*fC-Rd;

    //--- Calculate error and update --- //

    float resid = 0.75f*gM*(CO2-Ci) - A - Rd;


    return resid;

}

float PhotosynthesisModel::evaluateEmpiricalModel( float i_PAR, float TL, float CO2, float gM ){

    //initial guess for intercellular CO2
    float Ci = CO2;

    //--- Light Response Function --- //

    float fL = i_PAR/(empiricalmodelcoeffs.theta+i_PAR);

    assert( fL>=0 && fL<=1 );

    //--- Respiration Rate --- //

    float Rd = empiricalmodelcoeffs.R*sqrt(TL-273.f)*exp(-empiricalmodelcoeffs.ER/TL);

    float Ci_old = Ci;
    float Ci_old_old = 0.95f*Ci;

    float resid_old = evaluateCi_Empirical( Ci_old, CO2, fL, Rd, gM );
    float resid_old_old = evaluateCi_Empirical( Ci_old_old, CO2, fL, Rd, gM );

    float err = 10000, err_max = 0.01;
    int iter = 0, max_iter = 100;
    float resid;
    while( err>err_max && iter<max_iter ){

        if( resid_old==resid_old_old ){//this condition will cause NaN
            break;
        }

        Ci = fabs((Ci_old_old*resid_old-Ci_old*resid_old_old)/(resid_old-resid_old_old));

        resid = evaluateCi_Empirical( Ci, CO2, fL, Rd, gM );

        resid_old_old = resid_old;
        resid_old = resid;

        err = fabs(resid);

        Ci_old_old = Ci_old;
        Ci_old = Ci;

        iter++;

    }

    float A;
    if( err>err_max ){
        A = 0;
    }else{
        float fC = empiricalmodelcoeffs.kC*Ci/empiricalmodelcoeffs.Ci_ref;
        A = empiricalmodelcoeffs.Asat*fL*fC-Rd;
    }

    return A;

}

//float PhotosynthesisModel::evaluateCi_Farquhar( const float Ci, const float CO2, const float i_PAR, const float TL, const float gM, float& A, int& limitation_state ) const{
float PhotosynthesisModel::evaluateCi_Farquhar( float Ci, std::vector<float> &variables, const void* parameters ){

//    std::vector<float> variables_v = *reinterpret_cast<std::vector<float>*>(variables);
    const FarquharModelCoefficients modelcoeffs = *reinterpret_cast<const FarquharModelCoefficients*>(parameters);

    float CO2 = variables[0];
    float i_PAR = variables[1];
    float TL = variables[2];
    float gM = variables[3];

//    printf("Variables = [%f %f %f %f]\n",CO2, i_PAR, TL, gM);

    //molar gas constant (kJ/K/mol)
    float R = 0.0083144598;

    float Rd = modelcoeffs.Rd*exp(modelcoeffs.c_Rd-modelcoeffs.dH_Rd/(R*TL));
    float Vcmax = modelcoeffs.Vcmax*exp(modelcoeffs.c_Vcmax-modelcoeffs.dH_Vcmax/(R*TL));
    float Jmax = modelcoeffs.Jmax*exp(modelcoeffs.c_Jmax-modelcoeffs.dH_Jmax/(R*TL));

    float Gamma = exp(modelcoeffs.c_Gamma-modelcoeffs.dH_Gamma/(R*TL));
    float Kc = exp(modelcoeffs.c_Kc-modelcoeffs.dH_Kc/(R*TL));
    float Ko = exp(modelcoeffs.c_Ko-modelcoeffs.dH_Ko/(R*TL));

    float Kco = Kc*(1.f+modelcoeffs.O/Ko);

    float Wc = Vcmax*Ci/(Ci+Kco);

    float J = Jmax*i_PAR*modelcoeffs.alpha/(i_PAR*modelcoeffs.alpha+Jmax);
    float Wj = J*Ci/(4.f*Ci+8.f*Gamma);

    float A = (1-Gamma/Ci)*fmin(Wc,Wj)-Rd;

    float limitation_state;
    if( Wj<Wc ){ //light limited
        limitation_state = 0;
    }else{ //CO2 limited
        limitation_state = 1;
    }

    //--- Calculate error and update --- //

    float resid = 0.75f*gM*(CO2-Ci) - A;

    variables[4] = A;
    variables[5] = limitation_state;

    return resid;

}

float PhotosynthesisModel::evaluateFarquharModel( float i_PAR, float TL, float CO2, float gM, float& Ci, int& limitation_state ){

    float A = 0;
    Ci = 100;

    std::vector<float> variables{CO2, i_PAR, TL, gM, A, float(limitation_state)};

    Ci = fzero( evaluateCi_Farquhar, variables, &farquharmodelcoeffs, Ci );

    A = variables[4];
    limitation_state = (int)variables[5];

    return A;

}

EmpiricalModelCoefficients PhotosynthesisModel::getEmpiricalModelCoefficients(){
    return empiricalmodelcoeffs;
}

FarquharModelCoefficients PhotosynthesisModel::getFarquharModelCoefficients(){
    return farquharmodelcoeffs;
}

void PhotosynthesisModel::optionalOutputPrimitiveData( const char* label ){

    if( strcmp(label,"Ci")==0 || strcmp(label,"limitation_state")==0 ){
        output_prim_data.emplace_back(label );
    }else{
        std::cout << "WARNING (PhotosynthesisModel::optionalOutputPrimitiveData): unknown output primitive data " << label << std::endl;
    }

}
