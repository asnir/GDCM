/*=========================================================================

  Program: GDCM (Grassroots DICOM). A DICOM library
  Module:  $URL$

  Copyright (c) 2006-2008 Mathieu Malaterre
  All rights reserved.
  See Copyright.txt or http://gdcm.sourceforge.net/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "gdcmReader.h"
#include "gdcmWriter.h"
#include "gdcmFilename.h"
#include "gdcmSystem.h"
#include "gdcmTesting.h"

//bool IsImpossibleToRewrite(const char *filename)
//{
//  const char *impossible;
//  int i = 0;
//  while( (impossible= gdcmBlackListWriterDataImages[i]) )
//    {
//    if( strcmp( impossible, filename ) == 0 )
//      {
//      return true;
//      }
//    ++i;
//    }
//  return false;
//}

namespace gdcm
{
// This is the full md5 of the rewriten file. The file was manually check
// and is (should be) exactly what should have been written in the first place
// test was done using dcmtk 3.5.4 / dicom3tools
static const char * const gdcmMD5DataBrokenImages[][2] = { 
// file has some garbage at the end, replace with a trailing end item.
{ "e8ed75f5e13cc20e96ee716bcc78351b" , "gdcm-JPEG-LossLess3a.dcm" }, // size match

// files are little endian implicit meta data header:
{ "8cb29ba0173c66e7adb4c54c6b0a5896" , "GE_DLX-8-MONO2-PrivateSyntax.dcm" },
{ "ed93b34819bf2acbacefb510476e8d4a" , "PICKER-16-MONO2-No_DicomV3_Preamble.dcm" },

// stupidest file ever, 0x6 was sent in place of 0x4 ... sigh
{ "c8cce480eac80770a3c6e456c7d8d66f" , "SIEMENS_MAGNETOM-12-MONO2-Uncompressed.dcm" }, // size match

// big endian / little endian nightmare from PMS
{ "df0e01aae299317db1719e4de72b8937" , "MR_Philips_Intera_SwitchIndianess_noLgtSQItem_in_trueLgtSeq.dcm" }, // size match and CheckbigEndian match => ok !
{ "df632a3b5ca38340faa612a23b907ac4" , "PHILIPS_Intera-16-MONO2-Uncompress.dcm" }, // size match and CheckbigEndian match => ok !
{ "6bf002815fad665392acab24f09caa5e" , "MR_Philips_Intera_PrivateSequenceExplicitVR_in_SQ_2001_e05f_item_wrong_lgt_use_NOSHADOWSEQ.dcm" }, // size match and CheckbigEndian match => ok !

// little endian implicit meta header + a couple of attribute sent with correct, but odd length:
{ "e1b2956f781685fc9e46e0da26b8a0fd" , "THERALYS-12-MONO2-Uncompressed-Even_Length_Tag.dcm" },

// name says it all. dcmtk does not support this. dicom3tools confirmed that dataset is compatible
{ "fa34d4886d9fce1cdf5e10f7d5abd2cc" , "ExplicitVRforPublicElementsImplicitVRforShadowElements.dcm" }, // size mismatch

// item length are supposed to be 0, not FFFF...
{ "3cc629fa470efb114a14ca3909117eb8" , "SIEMENS-MR-RGB-16Bits.dcm" }, // size match


// this is a private syntax from ge with a little endian dataset and big endian pixel data
// FMI is little endian implicit, it also contained a couple of odd length attributes.
// using dcmtk it looks like file are identical
{ "2d23a8d55425c88bdd5e90f866a11607" , "PrivateGEImplicitVRBigEndianTransferSyntax16Bits.dcm" }, // size mismatch

// couple of weird stuff going on... dcdump confirmed dataset is identical in both file
{ "34abc36682a6e6ba22d7295931b39d85" , "DMCPACS_ExplicitImplicit_BogusIOP.dcm" }, // size mismatch

// weird stuff going on. dcdump confirmed dataset are identical. dcmdump can read output, and size match.
{ "82fda19e1f2046a289fe1307b70510af" , "gdcm-MR-PHILIPS-16-Multi-Seq.dcm" },

// yet another stupidest ever bug, 0xd was replaced with 0xa ... don't ask
{ "a047110a3935dc0fdda24d3b9e4769af" , "GE_GENESIS-16-MONO2-WrongLengthItem.dcm" }, // size match

// empty 16bits after VR should be 0 not garbage...
{ "6cded0f160edfab809cddfce2d562671" , "JDDICOM_Sample2.dcm" }, // size match

// simple issue, last fragment is odd, simply need to pad (easy, should be handle by most implementation)
{ "6234a50361f02cb9a739755d63cdd673" , "00191113.dcm" }, // size mismatch (obviously)
{ "28f9d4114b0699630a77d027910e5e41" , "MARCONI_MxTWin-12-MONO2-JpegLossless-ZeroLengthSQ.dcm" },

// serious bug from gdcm 1.2.0, where VR=UN would be written on 16bits length sigh... no toolkit will ever be able to deal with that thing (and should not anyway)
{ "50752239f24669697897c4b6542bc161" , "SIEMENS_MAGNETOM-12-MONO2-GDCM12-VRUN.dcm" },

// unordered dataset
{ "f221e76c6f0758877aa3cf13632480f4" , "dicomdir_Pms_WithVisit_WithPrivate_WithStudyComponents" }, // size match

// frankenstein-type dicom file:
// 1. Implicit encoding is used any time it is not known (most of the time, private element)
// 2. > (0x2001,0x1068) SQ ?   VR=<SQ>   VL=<0xffffffff> is sent twice (don't ask), second entry cannot be stored...
// dcdump kindda show file are somewhat compatible.
{ "69ca7a4300967cf2841da34d7904c6c4" , "TheralysGDCM120Bug.dcm" }, // size mismatch


{ 0 ,0 }
};

const char * GetMD5FromBrokenFile(const char *filepath)
{
  int i = 0;
  Testing::MD5DataImagesType md5s = gdcmMD5DataBrokenImages; //GetMD5DataImages();
  const char *p = md5s[i][1];
  Filename comp(filepath);
  const char *filename = comp.GetName();
  while( p != 0 )
    {
    if( strcmp( filename, p ) == 0 )
      {
      break;
      }
    ++i;
    p = md5s[i][1];
    }
  return md5s[i][0];
}

int TestWrite(const char *subdir, const char* filename, bool recursing, bool verbose = false)
{
  Reader reader;
  reader.SetFileName( filename );
  if ( !reader.Read() )
    {
    std::cerr << "Failed to read: " << filename << std::endl;
    return 1;
    }

  // Create directory first:
  std::string tmpdir = Testing::GetTempDirectory( subdir );
  if( !System::FileIsDirectory( tmpdir.c_str() ) )
    {
    System::MakeDirectory( tmpdir.c_str() );
    //return 1;
    }
  std::string outfilename = Testing::GetTempFilename( filename, subdir );

  Writer writer;
  writer.SetFileName( outfilename.c_str() );
  writer.SetFile( reader.GetFile() );
  writer.SetCheckFileMetaInformation( false );
  if( !writer.Write() )
    {
    std::cerr << "Failed to write: " << outfilename << std::endl;
    return 1;
    }

  // Ok we have now two files let's compare their md5 sum:
  char digest[33], outdigest[33];
  Testing::ComputeFileMD5(filename, digest);
  Testing::ComputeFileMD5(outfilename.c_str(), outdigest);
  if( strcmp(digest, outdigest) )
    {
	  if (recursing)
		  return 1;
    // too bad the file is not identical, so let's be paranoid and
    // try to reread-rewrite this just-writen file:
    // TODO: Copy file System::CopyFile( );
    std::string subsubdir = subdir;
    subsubdir += "/";
    subsubdir += subdir;
    if( TestWrite(subsubdir.c_str(), outfilename.c_str(), true ) )
      {
      std::cerr << filename << " and "
        << outfilename << " are different\n";
      return 1;
      }
   const char * ref = GetMD5FromBrokenFile(filename);
   if( ref )
     {
     if( strcmp(ref, outdigest) == 0 )
       {
       // ok this situation was already analyzed and the writen file is
       // readable by dcmtk and such
       size_t size1 = System::FileSize( filename );
       size_t size2 = System::FileSize( outfilename.c_str() );
       //assert( size1 == size2 ); // cannot deal with implicit VR meta data header
       return 0;
       }
      std::cerr << "incompatible ref:" << ref << " vs " << outdigest << " for file: " << filename << " & " << outfilename << std::endl;
      return 1; // ref exist but does not match, how is that possible ?
     }
   //if( !ref ) 
   //  {
   //  return 1;
   //  }
    // In theory I need to compare the two documents to check they
    // are identical... TODO
    std::cerr << filename << " and "
      << outfilename << " are different, output can be read though. Need manual intervention\n";
    return 1;
    }
  else
    {
    size_t size1 = System::FileSize( filename );
    size_t size2 = System::FileSize( outfilename.c_str() );
    assert( size1 == size2 );
    if(verbose)
    std::cerr << filename << " and " << outfilename << " are identical\n";
    return 0;
    }
}
}

int TestWriter(int argc, char *argv[])
{
  if( argc == 2 )
    {
    const char *filename = argv[1];
    return gdcm::TestWrite(argv[0], filename, false, true);
    }

  // else
  int r = 0, i = 0;
  gdcm::Trace::DebugOff();
  gdcm::Trace::WarningOff();
  const char *filename;
  const char * const *filenames = gdcm::Testing::GetFileNames();
  while( (filename = filenames[i]) )
    {
    r += gdcm::TestWrite(argv[0], filename, false );
    ++i;
    }

  return r;
}
