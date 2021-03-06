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
#include "FastXML2Inline.h"

FastXML2Inline::FastXML2Inline(const char*           filebase,
                               templates_registry_t& registry)
  : FastCodeGenBase(filebase, ".inl")
  , registry_(registry)
{
}

void FastXML2Inline::restore_scope(const std::string& name_attr)
{
  // we need to remove the postfix "xxx_cref::" or "xxx_mref::"
  // from cref_scope_ and mref_scope_
  std::size_t substract_size =  name_attr.size() + sizeof("_cref::")-1;
  std::string str = cref_scope_.str();
  str.resize(str.size() - substract_size);
  cref_scope_.clear();
  cref_scope_.str(str);
  cref_scope_.seekp(str.size());

  str = mref_scope_.str();
  str.resize(str.size() - substract_size);
  mref_scope_.clear();
  mref_scope_.str(str);
  mref_scope_.seekp(str.size());

}

bool FastXML2Inline::VisitEnterTemplate (const XMLElement & /* element */,
                                         const std::string& name_attr,
                                         std::size_t /* index */)
{
  out_ << "\ninline\n"
       << name_attr << "_cref::" << name_attr << "_cref(\n"
       << "  const mfast::value_storage* storage_array,\n"
       << "  instruction_cptr            instruction)\n"
       << "  : mfast::aggregate_cref(storage_array, instruction)\n"
       << "{\n"
       << "}\n\n"
       << "inline\n"
       << name_attr << "_cref::" << name_attr << "_cref(\n"
       << "  const mfast::field_cref& other)\n"
       << "  : mfast::aggregate_cref(mfast::detail::field_storage_helper::storage_ptr_of(other)->of_group.content_,\n"
       << "                          static_cast<instruction_cptr>(other.instruction()))\n"
       << "{\n"
       << "}\n\n"
       << "inline\n"
       << name_attr << "_mref::" << name_attr << "_mref(\n"
       << "  mfast::allocator*     alloc,\n"
       << "  mfast::value_storage* storage_array,\n"
       << "  instruction_cptr      instruction)\n"
       << "  : " << name_attr << "_mref_base(alloc, storage_array, instruction)\n"
       << "{\n"
       << "}\n\n"
       << "inline\n"
       << name_attr << "_mref::" << name_attr << "_mref(\n"
       << "  const mfast::field_mref_base& other)\n"
       << "  : " << name_attr << "_mref_base(other.allocator(),\n"
       << "                                  mfast::detail::field_storage_helper::storage_ptr_of(other)->of_group.content_,\n"
       << "                                  static_cast<instruction_cptr>(other.instruction()))\n"
       << "{\n"
       << "}\n\n"
       << "inline\n"
       << name_attr << "::" << name_attr << "(\n"
       << "  mfast::allocator* alloc)\n"
       << "  : mfast::message_type(alloc, instruction(), this->data())\n"
       << "{\n"
       << "}\n\n"
       << "inline\n"
       << name_attr << "::" << name_attr << "(\n"
       << "  const " << name_attr << "_cref& other,\n"
       << "  mfast::allocator* alloc)\n"
       << ": mfast::message_type(alloc, instruction())\n"
       << "{\n"
       << "  this->instruction()->copy_construct_value(my_storage_, this->data(), alloc, other.field_storage(0));\n"
       << "}\n\n"
       << "inline "<< name_attr << "::cref_type\n"
       << name_attr << "::ref() const\n"
       << "{\n"
       << "  return " << name_attr << "::cref_type(my_storage_.of_group.content_, instruction());\n"
       << "}\n\n"
       << "inline "<< name_attr << "::cref_type\n"
       << name_attr << "::cref() const\n"
       << "{\n"
       << "  return " << name_attr << "::cref_type(my_storage_.of_group.content_, instruction());\n"
       << "}\n\n"
       << "inline "<< name_attr << "::mref_type\n"
       << name_attr << "::ref()\n"
       << "{\n"
       << "  return " << name_attr << "::mref_type(alloc_, my_storage_.of_group.content_, instruction());\n"
       << "}\n\n"
       << "inline "<< name_attr << "::mref_type\n"
       << name_attr << "::mref()\n"
       << "{\n"
       << "  return " << name_attr << "::mref_type(alloc_, my_storage_.of_group.content_, instruction());\n"
       << "}\n\n";

  cref_scope_ << name_attr << "_cref::";
  mref_scope_ << name_attr << "_mref::";
  return out_.good();
}

bool FastXML2Inline::VisitExitTemplate (const XMLElement & /* element */,
                                        const std::string& name_attr,
                                        std::size_t /* numFields */,
                                        std::size_t /* index */)
{

  restore_scope(name_attr);
  return out_.good();
}

bool FastXML2Inline::VisitEnterGroup (const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t        index)
{

  std::string only_templateRef_subindex;
  const XMLElement * templateRef_child = only_child_templateRef(element);
  
  if (templateRef_child && get_optional_attr(*templateRef_child, "name",0) == 0) {
    out_ << "\ninline " << cref_scope_.str() << name_attr << "_cref\n"
         << cref_scope_.str() << "get_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << cref_scope_.str() << name_attr << "_cref>( mfast::aggregate_cref((*this)[" << index << "])[0] );\n"
         << "}\n\n"
         << "inline " << mref_scope_.str() << name_attr << "_mref\n"
         << mref_scope_.str() << "set_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << mref_scope_.str() << name_attr << "_mref>( mfast::aggregate_mref((*this)[" << index << "])[0] );\n"
         << "}\n\n";
    
  }
  else {
    out_ << "\ninline " << cref_scope_.str() << name_attr << "_cref\n"
         << cref_scope_.str() << "get_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << cref_scope_.str() << name_attr << "_cref>((*this)[" << index << "]);\n"
         << "}\n\n"
         << "inline " << mref_scope_.str() << name_attr << "_mref\n"
         << mref_scope_.str() << "set_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << mref_scope_.str() << name_attr << "_mref>((*this)[" << index << "]);\n"
         << "}\n\n";
  }

  if (templateRef_child == 0) {

    out_ << "inline\n"
         << cref_scope_.str() << name_attr << "_cref::"<< name_attr << "_cref(\n"
         << "  const mfast::value_storage*   storage,\n"
         << "  " << cref_scope_.str() << name_attr << "_cref::instruction_cptr instruction)\n"
         << "  : mfast::group_cref(storage, instruction)\n"
         << "{\n"
         << "}\n\n"
         << "inline\n"
         << cref_scope_.str() << name_attr << "_cref::"<< name_attr << "_cref(\n"
         << "  const mfast::field_cref& other)\n"
         << "  : mfast::group_cref(other)\n"
         << "{\n"
         << "}\n\n"
         << "inline\n"
         << mref_scope_.str() << name_attr << "_mref::"<< name_attr << "_mref(\n"
         << "  mfast::allocator*      alloc,\n"
         << "  mfast::value_storage*  storage,\n"
         << "  " << mref_scope_.str() << name_attr << "_mref::instruction_cptr instruction)\n"
         << "  : " << mref_scope_.str() << name_attr << "_mref_base(alloc, storage, instruction)\n"
         << "{\n"
         << "}\n"
         << "inline\n"
         << mref_scope_.str() << name_attr << "_mref::"<< name_attr << "_mref(\n"
         << "  const mfast::field_mref_base& other)\n"
         << "  : " << mref_scope_.str() << name_attr << "_mref_base(other)\n"
         << "{\n"
         << "}\n";
    cref_scope_ << name_attr << "_cref::";
    mref_scope_ << name_attr << "_mref::";
    return true;
  }

  return false;
}

bool FastXML2Inline::VisitExitGroup (const XMLElement & element,
                                     const std::string& name_attr,
                                     std::size_t /* numFields */,
                                     std::size_t /* index */)
{
  if (only_child_templateRef(element) == 0) {
    restore_scope(name_attr);
  }
  return true;
}

bool FastXML2Inline::VisitEnterSequence (const XMLElement & element,
                                         const std::string& name_attr,
                                         std::size_t        index)
{
  out_ << "\ninline " << cref_scope_.str() << name_attr << "_cref\n"
       << cref_scope_.str() << "get_" << name_attr << "() const\n"
       << "{\n"
       << "  return static_cast<" << cref_scope_.str() << name_attr << "_cref>((*this)[" << index << "]);\n"
       << "}\n\n"
       << "inline " << mref_scope_.str() << name_attr << "_mref\n"
       << mref_scope_.str() << "set_" << name_attr << "() const\n"
       << "{\n"
       << "  return static_cast<" << mref_scope_.str() << name_attr << "_mref>((*this)[" << index << "]);\n"
       << "}\n\n";

  if (only_child(element) == 0) {
    out_ << "inline\n"
         << cref_scope_.str() << name_attr << "_element_cref::"<< name_attr << "_element_cref(\n"
         << "  const mfast::value_storage*   storage,\n"
         << "  " << cref_scope_.str() << name_attr << "_element_cref::instruction_cptr instruction)\n"
         << "  : " << cref_scope_.str() << name_attr << "_element_cref_base(storage, instruction)\n"
         << "{\n"
         << "}\n\n"
         << "inline\n"
         << mref_scope_.str() << name_attr << "_element_mref::"<< name_attr << "_element_mref(\n"
         << "  mfast::allocator*               alloc,\n"
         << "  mfast::value_storage*         storage,\n"
         << "  " << mref_scope_.str() << name_attr << "_element_mref::instruction_cptr instruction)\n"
         << "  : " << mref_scope_.str() << name_attr << "_element_mref_base(alloc,storage, instruction)\n"
         << "{\n"
         << "}\n";
    cref_scope_ << name_attr << "_element_cref::";
    mref_scope_ << name_attr << "_element_mref::";

    return true;
  }

  return false;
}

bool FastXML2Inline::VisitExitSequence (const XMLElement & element,
                                        const std::string& name_attr,
                                        std::size_t /* numFields */,
                                        std::size_t /* index */)
{
  if (only_child(element) == 0) {
    std::string name(name_attr);
    name += "_element";
    restore_scope(name);
  }
  return true;
}

bool FastXML2Inline::VisitEnterSimpleValue (const XMLElement & element,
                                            const char*        cpp_type,
                                            const std::string& name_attr,
                                            std::size_t        index)
{
  out_ << "\n"
       << "inline\n"
       << "mfast::" << cpp_type << "_cref\n"
       << cref_scope_.str() << "get_" << name_attr << "() const\n"
       << "{\n"
       << "  return static_cast<mfast::" << cpp_type << "_cref>((*this)[" << index << "]);\n"
       << "}\n\n";

  if (is_mandatory_constant(element))
    return true;

  out_ << "inline\n"
       << "mfast::"<< cpp_type << "_mref\n"
       << mref_scope_.str() << "set_" << name_attr << "() const\n"
       << "{\n"
       << "  return static_cast<mfast::" << cpp_type << "_mref>((*this)[" << index << "]);\n"
       << "}\n";
  return true;
}

bool FastXML2Inline::VisitString (const XMLElement & element,
                                  const std::string& name_attr,
                                  std::size_t        index)
{
  const char* charset = get_optional_attr(element, "charset", "ascii");
  std::string cpp_type = charset;
  cpp_type += "_string";
  VisitEnterSimpleValue(element, cpp_type.c_str(), name_attr, index);
  return true;
}

bool FastXML2Inline::VisitInteger (const XMLElement & element,
                                   int                bits,
                                   const std::string& name_attr,
                                   std::size_t        index)
{
  char buf[10];
  TIXML_SNPRINTF(buf, 10, "uint%d",bits);
  const char* cpp_type = (element.Name()[0] == 'u') ? buf : buf+1;
  VisitEnterSimpleValue(element, cpp_type, name_attr, index);
  return true;
}

bool FastXML2Inline::VisitDecimal (const XMLElement & element,
                                   const std::string& name_attr,
                                   std::size_t        index)
{
  VisitEnterSimpleValue(element, "decimal", name_attr, index);
  return true;
}

bool FastXML2Inline::VisitByteVector (const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t        index)
{
  VisitEnterSimpleValue(element, "byte_vector", name_attr, index);
  return true;
}

bool FastXML2Inline::VisitTemplateRef(const XMLElement & element,
                                      const std::string& name_attr,
                                      std::size_t        index)
{
  if (name_attr.size()) {
    std::string ns = get_optional_attr(element, "ns", current_context().ns_.c_str());
    templates_registry_t::iterator itr = registry_.find(ns + "||" + name_attr);
    std::string qulified_name = name_attr;
    if (itr != registry_.end()) {
      qulified_name = itr->second + "::" + name_attr;
    }

    out_ << "\n"
         << "inline\n"
         << qulified_name << "_cref\n"
         << cref_scope_.str() << "get_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << qulified_name << "_cref>((*this)[" << index << "]);\n"
         << "}\n\n";


    out_ << "inline\n"
         << qulified_name << "_mref\n"
         << mref_scope_.str() << "set_" << name_attr << "() const\n"
         << "{\n"
         << "  return static_cast<" << qulified_name << "_mref>((*this)[" << index << "]);\n"
         << "}\n";
  }
  else {
    out_ << "\n"
         << "inline\n"
         << "mfast::nested_message_cref\n"
         << cref_scope_.str() << "get_nested_message" << index << "() const\n"
         << "{\n"
         << "  return mfast::nested_message_cref((*this)[" << index << "]);\n"
         << "}\n\n";


    out_ << "inline\n"
         << "mfast::nested_message_mref\n"
         << mref_scope_.str() << "set_nested_message" << index << "() const\n"
         << "{\n"
         << "  return mfast::nested_message_mref((*this)[" << index << "]);\n"
         << "}\n";
  }
  return true;
}
