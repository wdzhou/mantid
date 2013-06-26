#ifndef MANTID_KERNEL_FILEDESCRIPTORTEST_H_
#define MANTID_KERNEL_FILEDESCRIPTORTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidKernel/FileDescriptor.h"
#include "MantidKernel/ConfigService.h"

#include <Poco/Path.h>
#include <Poco/File.h>

using Mantid::Kernel::FileDescriptor;

class FileDescriptorTest : public CxxTest::TestSuite
{
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static FileDescriptorTest *createSuite() { return new FileDescriptorTest(); }
  static void destroySuite( FileDescriptorTest *suite ) { delete suite; }

  FileDescriptorTest()
  {
    using Mantid::Kernel::ConfigService;
    auto dataPaths = ConfigService::Instance().getDataSearchDirs();
    for(auto it = dataPaths.begin(); it != dataPaths.end(); ++it)
    {
      Poco::Path nxsPath(*it, "CNCS_7860_event.nxs");
      if(Poco::File(nxsPath).exists()) m_testNexusPath = nxsPath.toString();
      Poco::Path nonNxsPath(*it, "CSP79590.raw");
      if(Poco::File(nonNxsPath).exists()) m_testNonNexusPath = nonNxsPath.toString();

      if(!m_testNexusPath.empty() && !m_testNonNexusPath.empty()) break;
    }
    if(m_testNexusPath.empty() || m_testNonNexusPath.empty())
    {
      throw std::runtime_error("Unable to find test files for FileDescriptorTest. "
          "The AutoTestData directory needs to be in the search path");
    }
  }

  //===================== Success cases ============================================
  void test_Constructor_With_Existing_File_Initializes_Description_Fields()
  {
    const std::string filename = m_testNexusPath;
    FileDescriptor descr(filename);

    TS_ASSERT_EQUALS(filename, descr.filename());
    TS_ASSERT_EQUALS(".nxs", descr.extension());
  }

  void test_File_Is_Opened_After_Object_Is_Constructed()
  {
    FileDescriptor descr(m_testNexusPath);
    TS_ASSERT(descr.data().is_open());
  }

  void test_Intial_Stream_Is_Positioned_At_Start_Of_File()
  {
    const std::string filename = m_testNexusPath;
    FileDescriptor descr(filename);

    auto & stream = descr.data();
    long int streamPos = stream.tellg();

    TS_ASSERT_EQUALS(0, streamPos);
  }

  void test_ResetStreamToStart_Repositions_Stream_Start_Of_File()
  {
    const std::string filename = m_testNexusPath;
    FileDescriptor descr(filename);
    auto & stream = descr.data();
    char byte('0');
    stream >> byte; // read byte from stream

    TS_ASSERT_EQUALS(1, stream.tellg());
    descr.resetStreamToStart();
    TS_ASSERT_EQUALS(0, stream.tellg());
  }

  //===================== Failure cases ============================================
  void test_Constructor_Throws_With_Empty_filename()
  {
    TS_ASSERT_THROWS(FileDescriptor(""), std::invalid_argument);
  }

  void test_Constructor_Throws_With_NonExistant_filename()
  {
    TS_ASSERT_THROWS(FileDescriptor("__ThisShouldBeANonExistantFile.txt"), std::invalid_argument);
  }

private:
  std::string m_testNexusPath;
  std::string m_testNonNexusPath;
};


#endif /* MANTID_KERNEL_FILEDESCRIPTORTEST_H_ */
