// Copyright (c) 2013, Huang-Ming Huang,  Object Computing, Inc.
// All rights reserved.
//
// This file is part of mFAST.
//
//     mFAST is free software: you can redistribute it and/or modify
//     it under the terms of the GNU Lesser General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     mFAST is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU Lesser General Public License
//     along with mFast.  If not, see <http://www.gnu.org/licenses/>.
//
#include "FastXML2Source.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
namespace {

struct cstr_compare
{
  bool operator()(const char* lhs, const char* rhs) const
  {
    return strcmp(lhs, rhs) < 0;
  }

};

struct map_value_type
{
  const char* first;
  const char* second;
};


struct is_space {
  bool operator() (char c) const
  {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      return true;
    }
    return false;
  }

};

struct decimal_value {
  int64_t mantissa;
  int32_t exponent;
};

inline std::ostream&
operator << (std::ostream & out, const decimal_value& in)
{
  out << in.mantissa << "LL, " << in.exponent;
  return out;
}

std::istream &
operator>>(std::istream &source, decimal_value& result)
{
  std::string decimal_string;
  source >> decimal_string;

  std::size_t float_pos = decimal_string.find_first_of('.');
  std::size_t nonzero_pos = decimal_string.find_last_not_of(".0");

  if (float_pos == std::string::npos || nonzero_pos < float_pos) {
    // this is an integer
    if (float_pos != std::string::npos) {
      decimal_string = decimal_string.substr(0, float_pos);
      nonzero_pos = decimal_string.find_last_not_of('0');
    }
    if (nonzero_pos == std::string::npos)
      nonzero_pos = 0;
    result.exponent = decimal_string.size()-1 - nonzero_pos;
    result.mantissa = boost::lexical_cast<int64_t>(decimal_string.substr(0, nonzero_pos+1));
  }
  else {
    decimal_string = decimal_string.substr(0, nonzero_pos+1);
    result.exponent = float_pos - decimal_string.size() +1;
    result.mantissa = boost::lexical_cast<int64_t>(decimal_string.erase(float_pos, 1));
  }
  return source;
}

class XMLFormatError
  : public std::exception
{
  static char buffer_[256];

  public:
    XMLFormatError(const char* format, ...)
    {
      va_list ap;
      va_start(ap, format);
      vsnprintf(buffer_, 256, format, ap);
      va_end(ap);
      buffer_[255] = 0;
    }

    const char* what() const throw ()
    {
      return buffer_;
    }

};

char XMLFormatError::buffer_[256];
}


void FastXML2Source::restore_scope(const std::string& name_attr)
{
  // we need to remove the postfix "xxx_cref::" or "xxx_mref::"
  // from cref_scope_ and mref_scope_
  std::size_t substract_size =  name_attr.size() + sizeof("_cref::")-1;
  std::string str = cref_scope_.str();
  str.resize(str.size() - substract_size);
  cref_scope_.clear();
  cref_scope_.str(str);
  cref_scope_.seekp(str.size());
}

std::string FastXML2Source::prefix_string() const
{
  std::stringstream strm;
  for (std::size_t i = 0; i < prefixes_.size(); ++i) {
    strm << prefixes_[i] << "__";
  }
  return strm.str();
}

FastXML2Source::FastXML2Source(const char* filebase, templates_registry_t& registry)
  : FastCodeGenBase(filebase, ".cpp")
  , registry_(registry)
{
}

/// Visit a document.
bool FastXML2Source::VisitEnter( const XMLDocument& /*doc*/ )
{
  out_<< "#include \"" << filebase_ << ".h\"\n"
      << "\n"
      << "using namespace mfast;\n\n"
      << "namespace " << filebase_ << "\n{\n";
  return out_.good();
}

/// Visit a document.
bool FastXML2Source::VisitExit( const XMLDocument& /*doc*/ )
{
  out_ << "\n}\n";
  return out_.good();
}

bool FastXML2Source::VisitExitTemplates (const XMLElement& element,
                                         std::size_t /* numFields */)
{
  std::string instructions = template_instructions_.str();
  if (instructions.size())
    instructions.resize(instructions.size() - 2);

  out_ << "const template_instruction* "<< filebase_ << "_templates_instructions[] ={\n"
       << instructions
       << "};\n\n";

  out_ << "mfast::templates_description* description()\n"
       << "{\n"
       << "  static mfast::templates_description desc(\n"
       << "    \"" << get_optional_attr(element, "ns", "")  << "\", // ns\n"
       << "    \"" << get_optional_attr(element, "templateNs", "")<< "\", // templateNs\n"
       << "    \"" << get_optional_attr(element, "dictionary", "") << "\", // dictionary\n"
       << "    " << filebase_ << "_templates_instructions);\n"
       << "  return &desc;\n"
       << "}\n\n";


  return out_.good();
}

const XMLElement* FastXML2Source::fieldOpElement(const XMLElement & element)
{
  static const char* field_op_names[] = {
    "constant","default","copy","increment","delta","tail"
  };

  static std::set<const char*, cstr_compare> field_op_set (field_op_names, field_op_names + 6);

  for (const XMLElement* child = element.FirstChildElement(); child != 0; child = child->NextSiblingElement())
  {
    if (field_op_set.count(child->Name())) {
      return child;
    }
  }
  return 0;
}

void FastXML2Source::add_to_instruction_list(const std::string & name_attr)
{
  std::stringstream strm;
  strm << "  &" << prefix_string() << name_attr << "_instruction,\n";
  subinstructions_list_.back() += strm.str();
}

bool FastXML2Source::output_typeref(const XMLElement & element)
{
  std::string typeRef_name;
  std::string typeRef_ns;

  const XMLElement* typeRefElem = element.FirstChildElement("typeRef");
  if (typeRefElem) {
    typeRef_name = get_optional_attr(*typeRefElem, "name", "");
    typeRef_ns = get_optional_attr(*typeRefElem, "ns", "");
  }

  out_ << "  \"" << typeRef_name << "\", // typeRef name \n"
       << "  \"" << typeRef_ns << "\"); // typeRef ns \n\n";
  return out_.good();
}

bool FastXML2Source::VisitEnterTemplate (const XMLElement & /* element */,
                                         const std::string& name_attr,
                                         std::size_t /* index */)
{
  std::string qulified_name = current_context().ns_ + "||" + name_attr;
  registry_[qulified_name] = filebase_;

  prefixes_.push_back(name_attr);
  // out_ << "namespace " << name_attr << "_def\n"
  //      << "{\n";

  subinstructions_list_.resize(subinstructions_list_.size()+1);
  cref_scope_ << name_attr << "_cref::";

  out_ << "const " << name_attr << "::instruction_type*\n"
       << name_attr << "::instruction()\n"
       << "{\n";

  return out_.good();
}

bool FastXML2Source::VisitExitTemplate (const XMLElement & element,
                                        const std::string& name_attr,
                                        std::size_t        numFields,
                                        std::size_t /* index */)
{
  restore_scope(name_attr);


  bool reset = false;
  const XMLAttribute* reset_attr = element.FindAttribute("scp:reset");
  if (reset_attr == 0)
    reset_attr = element.FindAttribute("reset");

  if (reset_attr) {
    if (strcmp(reset_attr->Value(), "true") == 0 || strcmp(reset_attr->Value(), "yes") == 0)
      reset = true;
  }

  output_subinstructions(name_attr);
  prefixes_.pop_back();

  template_instructions_ << "  " << name_attr << "::instruction(),\n";



  out_ << "  const static " << name_attr << "::instruction_type the_instruction(\n"
       << "    " << get_optional_attr(element, "id", "0") << ", // id\n"
       << "    \"" << name_attr << "\", // name\n"
       << "    \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "    \""<< get_optional_attr(element, "templateNs", "") << "\", // templateNs\n"
       << "    \""<< get_optional_attr(element, "dictionary", "") << "\", // dictionary\n"
       << "    "<< name_attr << "__subinstructions,\n"
       << "    "<< numFields << ", // num_fields\n"
       << "    " << reset << ", // reset\n";

  output_typeref(element);

  out_ << "  return &the_instruction;\n"
       << "}\n\n";
  return out_.good();
}

bool FastXML2Source::VisitEnterGroup (const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t /* index */)
{
  cref_scope_ << name_attr << "_cref::";
  // prefixes_.push_back(name_attr);
  // out_ << "namespace " << name_attr << "_def\n"
  //      << "{\n";

  add_to_instruction_list(name_attr);
  prefixes_.push_back(name_attr);

  if (only_child_templateRef(element) == 0) {
    subinstructions_list_.resize(subinstructions_list_.size()+1);
    return out_.good();
  }
  return false;
}

std::string
FastXML2Source::get_subinstructions(const XMLElement & element,
                                    const std::string& name_attr,
                                    std::size_t        numFields)
{
  const XMLElement* child = only_child_templateRef(element);

  std::stringstream subinstruction_arg;

  if (child == 0) {
    output_subinstructions(name_attr);
    // out_ << "} // namespace " << name_attr << "\n\n";

    subinstruction_arg << "  "<< prefix_string() << "subinstructions,\n"
                       << "  "<< numFields << ", // num_fields\n";
    prefixes_.pop_back();
  }
  else {
    // out_ << "} // namespace " << name_attr << "_def\n\n";
    prefixes_.pop_back();

    const char* templateRef_name = child->Attribute("name", 0);
    if (templateRef_name) {
      std::string ns = get_optional_attr(*child, "ns", current_context().ns_.c_str());
      templates_registry_t::iterator itr = registry_.find(ns + "||" + templateRef_name);
      std::string qulified_name = templateRef_name;
      if (itr != registry_.end()) {
        qulified_name = itr->second + "::" + templateRef_name;
      }

      subinstruction_arg << "  "<< qulified_name << "::instruction()->subinstructions(),\n"
                         << "  "<< qulified_name << "::instruction()->subinstructions_count(),\n";
    }
    else {
      const char*  presence_str = "mandatory";

      if (strcmp(element.Name(), "group") == 0) {
        presence_str = get_optional_attr(element, "presence", "mandatory");
      }

      // use templateref instruction singleton
      subinstruction_arg << "  "<< "mfast::templateref_instruction::default_instructions( presence_" << presence_str << "),\n"
                         << "  1, // num_fields\n";
    }
  }
  return subinstruction_arg.str();
}

bool FastXML2Source::VisitExitGroup (const XMLElement & element,
                                     const std::string& name_attr,
                                     std::size_t        numFields,
                                     std::size_t        index)
{

  std::string subinstruction_arg = get_subinstructions(element, name_attr, numFields);

  restore_scope(name_attr);

  out_ << "const static mfast::group_field_instruction\n"
       << prefix_string() << name_attr << "_instruction(\n"
       << "  "<<  index << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
       << "  \""<< name_attr << "\", // name\n"
       << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "  \""<< get_optional_attr(element, "dictionary", "") << "\", // dictionary\n"
       << subinstruction_arg;

  return output_typeref(element);;
}

bool FastXML2Source::VisitEnterSequence (const XMLElement & element,
                                         const std::string& name_attr,
                                         std::size_t /* index */)
{

  cref_scope_ << name_attr << "_element_cref::";

  // out_ << "namespace " << name_attr << "_def\n"
  //      << "{\n";

  add_to_instruction_list(name_attr);
  prefixes_.push_back(name_attr);


  const XMLElement* length_element = element.FirstChildElement("length");

  std::string fieldOpName="none";
  std::string opContext="0";
  std::string initialValue="";
  std::string id="0";
  std::string length_name;
  std::string ns="";

  if (length_element) {
    get_field_attributes(*length_element, name_attr, fieldOpName, opContext, initialValue);

    length_name = get_optional_attr(*length_element, "name", "");
    id = get_optional_attr(*length_element, "id", "0");
    ns = get_optional_attr(*length_element, "ns", "");

    if (initialValue.size())
      initialValue += "U"; // add unsigned integer suffix
  }

  // length
  out_ << "static uint32_field_instruction\n"
       << prefix_string() << name_attr << "_length_instruction(\n"
       << "  0,"
       << "  "<< "operator_" << fieldOpName << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< id << ", // id\n"
       << "  \""<< length_name << "\", // name\n"
       << "  \""<< ns << "\", // ns\n"
       << "  "<< opContext << ",  // opContext\n"
       << "  int_value_storage<uint32_t>(" << initialValue << ")); // initial_value\n\n";

  if (only_child_templateRef(element) == 0) {
    subinstructions_list_.resize(subinstructions_list_.size()+1);
    return out_.good();
  }
  else {
    return false;
  }
}

bool FastXML2Source::VisitExitSequence (const XMLElement & element,
                                        const std::string& name_attr,
                                        std::size_t        numFields,
                                        std::size_t        index)
{
  std::string lengthInstruction;

  std::stringstream strm;
  strm << "&" << prefix_string()  << name_attr <<  "_length_instruction";
  lengthInstruction = strm.str();

  std::string subinstruction_arg = get_subinstructions(element, name_attr, numFields);

  out_ << "const static mfast::sequence_field_instruction\n"
       << prefix_string() << name_attr << "_instruction(\n"
       << "  "<<  index << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
       << "  \""<< name_attr << "\", // name\n"
       << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "  \""<< get_optional_attr(element, "dictionary", "") << "\", // dictionary\n"
       << subinstruction_arg
       << "  "<< lengthInstruction << ", // length\n";
  restore_scope(name_attr);
  return output_typeref(element);
}

bool FastXML2Source::get_field_attributes(const XMLElement & element,
                                          const std::string& name_attr,
                                          std::string&       fieldOpName,
                                          std::string&       opContext,
                                          std::string&       initialValue)
{
  fieldOpName = "none";
  opContext = "0";
  initialValue = "";

  const XMLElement* fieldOpElem = fieldOpElement(element);

  if (fieldOpElem) {
    fieldOpName = fieldOpElem->Name();

    std::string opContext_key = get_optional_attr(*fieldOpElem, "key", "");
    std::string opContext_dict = get_optional_attr(*fieldOpElem, "dictionary", "");
    if (!opContext_key.empty() || !opContext_dict.empty()) {

      out_ << "const static " << "op_context_t " << prefix_string() << name_attr << "_opContext ={\n"
           << "  \"" << opContext_key << "\", \n"
           << "  \""<< get_optional_attr(*fieldOpElem, "ns", "") << "\", \n"
           << "  \""<< opContext_dict << "\"};";

      opContext = "&";
      opContext += name_attr;
      opContext += "_opContext";
    }
    initialValue = get_optional_attr(*fieldOpElem, "value", "");
    return true;
  }
  return false;
}

bool FastXML2Source::VisitString (const XMLElement & element,
                                  const std::string& name_attr,
                                  std::size_t        index)
{
  std::string fieldOpName;
  std::string opContext;
  std::string initialValue;

  get_field_attributes(element, name_attr, fieldOpName, opContext, initialValue);

  std::string charset =  get_optional_attr(element, "charset", "ascii");
  out_ << "const static " << charset << "_field_instruction\n"
       << prefix_string() << name_attr << "_instruction(\n"
       << "  " << index << ",\n"
       << "  "<< "operator_" << fieldOpName << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
       << "  \""<< name_attr << "\", // name\n"
       << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "  "<< opContext << ",  // opContext\n";


  if (initialValue.size()) {
    out_ << "  "<< "string_value_storage(\""<< initialValue << "\"," <<  initialValue.size() << ")";
  }
  else {
    out_ << "  "<< "string_value_storage()";
  }

  if (charset == "unicode") {
    out_ << ", // initial value\n";
    const XMLElement* length_element = element.FirstChildElement("length");
    if (length_element) {
      out_ << "  " << get_optional_attr(*length_element, "id", "0") << ", // length id\n"
           << "  \"" <<  get_optional_attr(*length_element, "name", "") << "\", // length name\n"
           << "  \"" << get_optional_attr(*length_element, "ns", "") << "\"); // length ns\n\n";
    }
    else {
      out_ << "0,\"\",\"\"); // no length element\n\n";
    }
  }
  else {
    out_ << "); // initial value\n\n";
  }

  add_to_instruction_list(name_attr);
  return out_.good();
}

bool FastXML2Source::VisitInteger (const XMLElement & element,
                                   int                bits,
                                   const std::string& name_attr,
                                   std::size_t        index)
{
  std::string fieldOpName;
  std::string opContext;
  std::string initialValue;

  get_field_attributes(element, name_attr, fieldOpName, opContext, initialValue);

  bool is_unsigned = element.Name()[0] == 'u';

  char buf[10];
  TIXML_SNPRINTF(buf, 10, "uint%d",bits);
  const char* cpp_type = is_unsigned ? buf : buf+1;

  if (initialValue.size()) {
    const char* unsigned_suffix = is_unsigned ? "U" : "";
    const char* longlong_suffix = (bits == 64) ? "LL" : "";
    initialValue += unsigned_suffix;
    initialValue += longlong_suffix;
  }

  out_ << "const static " << cpp_type << "_field_instruction\n"
       << prefix_string() << name_attr << "_instruction(\n"
       << "  " << index << ",\n"
       << "  "<< "operator_" << fieldOpName << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
       << "  \""<< name_attr << "\", // name\n"
       << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "  "<< opContext << ",  // opContext\n"
       << "  int_value_storage<"<< cpp_type << "_t>(" << initialValue  <<  ")); // initial_value\n\n";

  add_to_instruction_list(name_attr);
  return out_.good();
}

bool FastXML2Source::VisitDecimal (const XMLElement & element,
                                   const std::string& name_attr,
                                   std::size_t        index)
{
  const XMLElement* mantissa_element = element.FirstChildElement("mantissa");
  const XMLElement* exponent_element = element.FirstChildElement("exponent");

  if (mantissa_element || exponent_element) {

    std::string mantissa_fieldOpName;
    std::string mantissa_opContext;
    std::string mantissa_initialValue;

    get_field_attributes(*mantissa_element,
                         name_attr + "_mantissa",
                         mantissa_fieldOpName,
                         mantissa_opContext,
                         mantissa_initialValue);

    std::string exponent_fieldOpName;
    std::string exponent_opContext;
    std::string exponent_initialValue;

    if (mantissa_initialValue.size())
      mantissa_initialValue += "LL";

    out_ << "static mantissa_field_instruction\n"
         << prefix_string() << name_attr << "_mantissa_instruction(\n"
         << "  "<< "operator_" << mantissa_fieldOpName << ",\n"
         << "  "<< mantissa_opContext << ",  // mantissa opContext\n"
         << "  int_value_storage<int64_t>("<< mantissa_initialValue << "));// mantissa inital value\n\n";

    get_field_attributes(*exponent_element,
                         name_attr + "_exponent",
                         exponent_fieldOpName,
                         exponent_opContext,
                         exponent_initialValue);

    if (exponent_initialValue.size()) {
      exponent_initialValue = std::string("decimal_value_storage(0,") + exponent_initialValue;
    }
    else {
      exponent_initialValue = "decimal_value_storage(";
    }

    out_ << "const static "<< "decimal_field_instruction\n"
         << prefix_string() << name_attr << "_instruction(\n"
         << "  " << index << ",\n"
         << "  "<< "operator_" << exponent_fieldOpName << ", // exponent\n"
         << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
         << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
         << "  \""<< name_attr << "\", // name\n"
         << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
         << "  "<< exponent_opContext << ",  // exponent opContext\n"
         << "  &" << prefix_string() << name_attr << "_mantissa_instruction,\n"
         << "  "<< exponent_initialValue << ")); // exponent initial_value\n\n";

  }
  else {

    std::string fieldOpName;
    std::string opContext;
    std::string initialValue;

    get_field_attributes(element, name_attr, fieldOpName, opContext, initialValue);

    if (initialValue.size()) {
      std::stringstream strm;
      strm << boost::lexical_cast<decimal_value>(initialValue);
      initialValue = strm.str();
    }

    try {

      out_ << "const static "<< "decimal_field_instruction\n"
           << prefix_string() << name_attr << "_instruction(\n"
           << "  " << index << ",\n"
           << "  "<< "operator_" << fieldOpName << ",\n"
           << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
           << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
           << "  \""<< name_attr << "\", // name\n"
           << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
           << "  "<< opContext << ",  // opContext\n"
           << "  decimal_value_storage("<< initialValue  << ")); // initial_value\n\n";

    }
    catch (boost::bad_lexical_cast&) {
      throw XMLFormatError("%s is not a valid decimal number", initialValue.c_str());
    }
  }

  add_to_instruction_list(name_attr);
  return out_.good();
}

bool FastXML2Source::VisitByteVector (const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t        index)
{
  std::string fieldOpName;
  std::string opContext;
  std::string initialValue;

  get_field_attributes(element, name_attr, fieldOpName, opContext, initialValue);

  if (initialValue.size()) {
    // remove all spaces
    std::string old = initialValue;
    initialValue.erase( std::remove_if(initialValue.begin(), initialValue.end(), is_space()), initialValue.end());
    initialValue.find_first_not_of("0123456789ABCDEFabcdef");
    std::size_t invalid_pos = initialValue.find_first_not_of("0123456789ABCDEFabcdef");

    if (invalid_pos != std::string::npos) {
      throw XMLFormatError("\"%s\" is not a valid byte vector, invalid character %c\n", old.c_str(), initialValue[invalid_pos] );
    }
    else if (initialValue.size() % 2) {
      throw XMLFormatError("\"%s\" is not a valid byte vector, it must have even digits\n", old.c_str());
    }

    std::string hex_string;
    hex_string.reserve(2* initialValue.size());

    std::string::const_iterator itr = initialValue.begin();

    while (itr != initialValue.end()) {
      const char* prefix = "\\x";
      hex_string.insert(hex_string.end(), prefix, prefix+2);
      hex_string.insert(hex_string.end(), itr, itr+2);
      itr += 2;
    }
    initialValue = hex_string;
  }

  out_ << "const static byte_vector_field_instruction\n"
       << prefix_string() << name_attr << "_instruction(\n"
       << "  " << index << ",\n"
       << "operator_" << fieldOpName << ",\n"
       << "  "<< "presence_" << get_optional_attr(element, "presence", "mandatory") << ",\n"
       << "  "<< get_optional_attr(element, "id", "0") << ", // id\n"
       << "  \""<< name_attr << "\", // name\n"
       << "  \""<< get_optional_attr(element, "ns", "") << "\", // ns\n"
       << "  "<< opContext << ",  // opContext\n";

  if (initialValue.size()) {
    out_ << "  "<< "byte_vector_value_storage(\""<< initialValue << "\"," <<  initialValue.size()/4 << ")";
  }
  else {
    out_ << "  "<< "byte_vector_value_storage()";
  }

  out_ << ", // initial_value\n";

  const XMLElement* length_element = element.FirstChildElement("length");
  if (length_element) {
    out_ << "  " << get_optional_attr(*length_element, "id", "0") << ", // length id\n"
         << "  \"" <<  get_optional_attr(*length_element, "name", "") << "\", // length name\n"
         << "  \"" << get_optional_attr(*length_element, "ns", "") << "\"); // length ns\n\n";

  }
  else {
    out_ << "0,\"\",\"\"); // no length element\n\n";
  }

  add_to_instruction_list(name_attr);
  return out_.good();
}

void FastXML2Source::output_subinstructions(const std::string /*name_attr*/)
{
  std::string content = subinstructions_list_.back();
  content.resize(content.size()-1);

  out_ << "const static field_instruction* " << prefix_string() << "subinstructions[] = {\n"
       << content << "\n"
       << "};\n\n";
  subinstructions_list_.pop_back();
}

bool FastXML2Source::VisitTemplateRef(const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t        index)
{
  if (name_attr.size()) {
    std::string ns = get_optional_attr(element, "ns", current_context().ns_.c_str());
    std::string cpp_namespace;

    templates_registry_t::const_iterator itr = registry_.find(ns+"||"+name_attr);
    if (itr != registry_.end()) {
      if (itr->second != filebase_) {
        cpp_namespace = itr->second + "::";
      }
    }
    else {
      std::stringstream err;
      err << "Error: Cannot find the definition for static templateRef name=\""
          << name_attr << "\", ns=\"" << ns << "\"\n";
      throw std::runtime_error(err.str());
    }

    out_ << "const templateref_instruction\n"
         << "templateref" << index << "_instruction(\n"
         << "  " << index << ",\n"
         << "  " << cpp_namespace << name_attr << "::instruction());\n\n";
  }
  else {
    out_ << "templateref_instruction\n"
         << "templateref" << index << "_instruction(\n"
         << "  " << index << ");\n\n";
  }

  std::stringstream tmp;
  tmp << "templateref" << index;
  add_to_instruction_list(tmp.str());

  return true;
}
