/*
Author : Jonas lamy
Based on work of Turetken & Fethallah Benmansour
*/

#include "itkOptimallyOrientedFlux.h"

#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkTimeProbe.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkMaskImageFilter.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "utils.h"

int main(int argc, char** argv)
{
    //********************************************************
    //                   Reading arguments
    //********************************************************
    bool isInputDicom;
 
    namespace po = boost::program_options;
    // parsing arguments
    po::options_description general_opt("Allowed options are ");
    general_opt.add_options()
    ("help,h", "display this message")
    ("input,i", po::value<std::string>(), "inputName : input img" )
    ("output,o", po::value<std::string>(), "ouputName : output img" )
    ("sigmaMin,m", po::value<double>(), "scale space sigma min")
    ("sigmaMax,M", po::value<double>(), "scale space sigma max")
    ("nbSigmaSteps,n",po::value<int>(),"nb steps sigma")
    ("sigma,s",po::value<double>(),"sigma for smoothing")
    ("inputIsDicom,d",po::bool_switch(&isInputDicom),"specify dicom input")
    ("mask,k",po::value<std::string>()->default_value(""),"mask response by image");

    bool parsingOK = true;
    po::variables_map vm;

    try{
      po::store(po::parse_command_line(argc,argv,general_opt),vm);
    }catch(const std::exception& ex)
    {
      parsingOK = false;
      std::cout<<"Error checking program option"<<ex.what()<< std::endl;
    }

    po::notify(vm);
    if( !parsingOK || vm.count("help") || argc<=1 )
    {
      std::cout<< general_opt << std::endl;
      return 0;
    }

    std::string inputFile = vm["input"].as<std::string>();
    std::string outputFile = vm["output"].as<std::string>();
    double sigmaMin = vm["sigmaMin"].as<double>();
    double sigmaMax = vm["sigmaMax"].as<double>();
    int nbSigmaSteps = vm["nbSigmaSteps"].as<int>();
    double fixedSigma = vm["sigma"].as<double>();
    std::string maskFile = vm["mask"].as<std::string>();

    //********************************************************
    //                    Reading inputs
    //********************************************************


    const unsigned int Dimension = 3;

    typedef double PixelType;
    typedef itk::Image<PixelType,Dimension> InputImageType;
    typedef itk::Image<uint8_t, Dimension> MaskImageType;

    InputImageType::Pointer inputImage = vUtils::readImage<InputImageType>(inputFile,isInputDicom);
    
    
    //********************************************************
    //                   Filter
    //********************************************************

    using GaussianFilterType = itk::DiscreteGaussianImageFilter<InputImageType,InputImageType>;
    auto gFilter = GaussianFilterType::New();
    gFilter->SetVariance(fixedSigma);
    gFilter->SetInput(inputImage);

    // creating radii from parameters

    double step = (sigmaMax - sigmaMin) / (double)(nbSigmaSteps-2+1);
    
    std::vector<double> radii;
    double i=sigmaMin;
    while(i<sigmaMax)
    {
      radii.push_back(i);
      i += step;
    }
    radii.push_back( sigmaMax );

    std::cout<<"scales: ";
    for(auto r :radii )
      std::cout<<r<<" ; ";
    std::cout<<std::endl;


    auto OOFfilter = itk::OptimallyOrientedFlux<InputImageType,InputImageType>::New();
    OOFfilter->SetInput( gFilter->GetOutput() );
    OOFfilter->SetRadii(radii);
    try{
        itk::TimeProbe timer;
        timer.Start();
        //
        OOFfilter->Update();

        //
        timer.Stop();
        std::cout<<"Computation time:"<<timer.GetMean()<<std::endl;
    }
    catch(itk::ExceptionObject &e)
    {
        std::cerr << e << std::endl;
    }

  
    auto rescaleFilter = itk::RescaleIntensityImageFilter<InputImageType>::New();
    
    MaskImageType::Pointer maskImage;
    if( !maskFile.empty() )
    {
      maskImage = vUtils::readImage<MaskImageType>(maskFile,isInputDicom);
    
      auto maskFilter = itk::MaskImageFilter<InputImageType,MaskImageType>::New();
      maskFilter->SetInput( OOFfilter->GetOutput() );
      maskFilter->SetMaskImage(maskImage);
      maskFilter->SetMaskingValue(0);
      maskFilter->SetOutsideValue(0);

      maskFilter->Update();
      rescaleFilter->SetInput( maskFilter->GetOutput() );
    }
    else{
      rescaleFilter->SetInput(OOFfilter->GetOutput());
    }
  
    rescaleFilter->SetOutputMinimum(0);
    rescaleFilter->SetOutputMaximum(1);
    

    typedef itk::ImageFileWriter<InputImageType> ScoreImageWriter;
    auto writer = ScoreImageWriter::New();
    writer->SetInput( rescaleFilter->GetOutput() );
    writer->SetFileName(outputFile);

    try{
        writer->Update();
    }
    catch(itk::ExceptionObject &e)
    {
        std::cerr << e << std::endl;
    }

    return 0;
}
