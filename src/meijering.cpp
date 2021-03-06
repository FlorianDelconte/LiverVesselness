#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

#include "itkMultiScaleHessianBasedMeasureImageFilter.h"
#include "itkHessianToMeijeringMeasureImageFilter.h"

#include "itkStatisticsImageFilter.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <string>

#include "utils.h"

int main( int argc, char* argv[] )
{
    bool isInputDicom;

    namespace po = boost::program_options;
    // parsing arguments
    po::options_description general_opt("Allowed options are ");
    general_opt.add_options()
    ("help,h", "display this message")
    ("input,i", po::value<std::string>(), "inputName : input img" )
    ("output,o", po::value<std::string>(), "ouputName : output img" )
    ("alpha,a", po::value<float>()->default_value(-0.33), "Meijering's alpha" )
    ("sigmaMin,m", po::value<float>(), "scale space sigma min")
    ("sigmaMax,M", po::value<float>(), "scale space sigma max")
    ("nbSigmaSteps,n",po::value<int>(),"nb steps sigma")
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
    float sigmaMin = vm["sigmaMin"].as<float>();
    float sigmaMax = vm["sigmaMax"].as<float>();
    int nbSigmaSteps = vm["nbSigmaSteps"].as<int>();
    float alpha = vm["alpha"].as<float>();
    std::string maskFile = vm["mask"].as<std::string>();

    constexpr unsigned int Dimension = 3;
    using PixelType = double;
    using ImageType = itk::Image< PixelType, Dimension >;

    using MaskImageType = itk::Image<uint8_t,Dimension>;

    ImageType::Pointer image = vUtils::readImage<ImageType>(inputFile,isInputDicom);
    MaskImageType::Pointer maskImage;

    using HessianPixelType = itk::SymmetricSecondRankTensor< double, Dimension >;
    using HessianImageType = itk::Image< HessianPixelType, Dimension >;
    using MaskImageType = itk::Image<uint8_t, Dimension>;
    
    using OutputImageType = itk::Image< double, Dimension >;

    using MeijeringFilterType = itk::HessianToMeijeringMeasureImageFilter<HessianImageType, OutputImageType,MaskImageType>;
    auto meijeringFilter = MeijeringFilterType::New();
    meijeringFilter->SetAlpha(alpha);
    
    if( !maskFile.empty() )
    {
      maskImage = vUtils::readImage<MaskImageType>(maskFile,isInputDicom);
      meijeringFilter->SetMaskImage(maskImage);
    }

    using MultiScaleEnhancementFilterType = itk::MultiScaleHessianBasedMeasureImageFilter< ImageType, HessianImageType, OutputImageType >;
    MultiScaleEnhancementFilterType::Pointer multiScaleEnhancementFilter =  MultiScaleEnhancementFilterType::New();
    multiScaleEnhancementFilter->SetInput( image );
    multiScaleEnhancementFilter->SetHessianToMeasureFilter( meijeringFilter );
    //multiScaleEnhancementFilter->SetSigmaStepMethodToLogarithmic();
    multiScaleEnhancementFilter->SetSigmaMinimum( sigmaMin );
    multiScaleEnhancementFilter->SetSigmaMaximum( sigmaMax );
    multiScaleEnhancementFilter->SetNumberOfSigmaSteps( nbSigmaSteps );

    auto stats = itk::StatisticsImageFilter<OutputImageType>::New();
    stats->SetInput(multiScaleEnhancementFilter->GetOutput());
    stats->Update();

    std::cout<<"multiscale"<<std::endl<<"min"
    <<stats->GetMinimum()<<std::endl
    <<"mean:"<<stats->GetMean()<<std::endl
    <<"max:"<<stats->GetMaximum()<<std::endl;

    using imageWriterType = OutputImageType;
    typedef  itk::ImageFileWriter< imageWriterType  > WriterType;
    WriterType::Pointer writer = WriterType::New();
    writer->SetInput( multiScaleEnhancementFilter->GetOutput() );
    writer->SetFileName( std::string(outputFile) );
    writer->Update();


    return EXIT_SUCCESS;
}
