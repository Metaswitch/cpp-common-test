/**
 * @file xml_utils_test.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include <string>
#include "gtest/gtest.h"
#include "basetest.hpp"
#include "xml_utils.h"

using namespace XMLUtils;

/// Fixture for XMLUtilsTest.
class XMLUtilsTest : public BaseTest
{
  XMLUtilsTest()
  {
  }

  virtual ~XMLUtilsTest()
  {
  }
};

TEST_F(XMLUtilsTest, ParseIntegerValid)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestInteger>1</TestInteger>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestInteger");
  int int_value = XMLUtils::parse_integer(test_node,
                                          "TestInteger",
                                          0,
                                          2);

  EXPECT_EQ(int_value, 1);

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseIntegerNaN)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestInteger>NaN</TestInteger>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestInteger");

  try
  {
    XMLUtils::parse_integer(test_node,
                            "TestInteger",
                            0,
                            2);
    FAIL() << "Expected xml_error";
  }
  catch (xml_error err)
  {
    // Ignore a shared iFC set ID which can't be parsed, and keep
    // going with the rest.
    std::string error_msg = err.what();
    EXPECT_EQ("Can't parse TestInteger as integer", error_msg);
  }

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseIntegerTooLarge)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestInteger>3</TestInteger>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestInteger");

  try
  {
    XMLUtils::parse_integer(test_node,
                            "TestInteger",
                            0,
                            2);
    FAIL() << "Expected xml_error";
  }
  catch (xml_error err)
  {
    std::string error_msg = err.what();
    EXPECT_EQ("TestInteger out of allowable range 0..2", error_msg);
  }

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseBoolTrue)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestBool>true</TestBool>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestBool");
  bool rc = XMLUtils::parse_bool(test_node, "TestBool");
  EXPECT_TRUE(rc);

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseBoolFalse)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestBool>false</TestBool>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestBool");
  bool rc = XMLUtils::parse_bool(test_node, "TestBool");
  EXPECT_FALSE(rc);

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseBoolMissing)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestBool>true</TestBool>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestNotBool");

  try
  {
    XMLUtils::parse_bool(test_node, "TestBool");
    FAIL() << "Expected xml_error";
  }
  catch (xml_error err)
  {
    std::string error_msg = err.what();
    EXPECT_EQ("Missing mandatory value for TestBool", error_msg);
  }

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseXMLStructure)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<TestNode1>"
                      "<TestNode2>string</TestNode2>"
                    "</TestNode1>"
                    "<TestBool></TestBool>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("TestNode1");

  std::string value = XMLUtils::get_first_node_value(test_node, "TestNode2");
  EXPECT_EQ(value, "string");

  std::string missing = XMLUtils::get_first_node_value(test_node, "TestNode3");
  EXPECT_EQ(missing, "");

  EXPECT_TRUE(XMLUtils::does_child_node_exist(test_node, "TestNode2"));
  EXPECT_FALSE(XMLUtils::does_child_node_exist(test_node, "TestNode3"));

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseNonDistinctIMPU)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<Extension>"
                    "  <IdentityType>3</IdentityType>"
                    "  <Extension>"
                    "    <Extension>"
                    "      <WildcardedIMPU>sip:wildcard</WildcardedIMPU>"
                    "    </Extension>"
                    "  </Extension>"
                    "</Extension>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("Extension");

  std::string identity = "unchanged";
  RegDataXMLUtils::parse_extension_identity(identity, test_node);
  EXPECT_EQ(identity, "sip:wildcard");

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseNonDistinctIMPUWrongIDType)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<Extension>"
                    "  <IdentityType>4</IdentityType>"
                    "  <Extension>"
                    "    <Extension>"
                    "      <WildcardedIMPU>sip:wildcard</WildcardedIMPU>"
                    "    </Extension>"
                    "  </Extension>"
                    "</Extension>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("Extension");

  std::string identity = "unchanged";
  RegDataXMLUtils::parse_extension_identity(identity, test_node);
  EXPECT_EQ(identity, "unchanged");

  free(cstr_xml);
}

TEST_F(XMLUtilsTest, ParseNonDistinctIMPUWrongIMPUType)
{
  std::shared_ptr<rapidxml::xml_document<> > root (new rapidxml::xml_document<>);

  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<Extension>"
                    "  <IdentityType>4</IdentityType>"
                    "  <Extension>"
                    "    <Extension>"
                    "      <DistinctIMPU>sip:distinct</DistinctIMPU>"
                    "    </Extension>"
                    "  </Extension>"
                    "</Extension>";

  char* cstr_xml = strdup(xml.c_str());
  root->parse<0>(cstr_xml);

  xml_node<>* test_node = root->first_node("Extension");

  std::string identity = "unchanged";
  RegDataXMLUtils::parse_extension_identity(identity, test_node);
  EXPECT_EQ(identity, "unchanged");

  free(cstr_xml);
}
