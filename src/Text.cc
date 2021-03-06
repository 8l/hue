// Copyright (c) 2012, Rasmus Andersson. All rights reserved. Use of this source
// code is governed by a MIT-style license that can be found in the LICENSE file.

#include "Text.h"

#include <fstream>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>
#include <llvm/ADT/OwningPtr.h>

namespace hue {


// Text(const UChar* text, size_t size, bool copy) : std::basic_string<UChar>() {
//   reserve(size);
//   UChar* data = const_cast<const UChar*>(data());
//   
// }

const Text Text::Empty;


Text& Text::appendUTF8String(const std::string& utf8string) throw(utf8::invalid_code_point) {
  utf8::utf8to32(utf8string.begin(), utf8string.end(), std::back_inserter(*this));
  return *this;
}


bool Text::setFromUTF8String(const std::string& utf8string) {
  clear();
  bool ok = true;
  try {
    utf8::utf8to32(utf8string.begin(), utf8string.end(), std::back_inserter(*this));
  } catch (const utf8::invalid_code_point &e) {
    clear();
    ok = false;
  }
  return ok;
}


bool Text::setFromUTF8Data(const uint8_t* data, const size_t length) {
  return setFromUTF8String(std::string(reinterpret_cast<const char*>(data), length));
}


bool Text::setFromUTF8InputStream(std::istream& is, size_t length) {
  if (!is.good()) return false;
  
  char *buf = NULL;
  std::string utf8string;
  
  if (length != 0) {
    buf = new char[length];
    is.read(buf, length);
    utf8string.assign(buf, length);
  } else {
    length = 4096;
    buf = new char[length];
    while (is.good()) {
      is.read(buf, length);
      utf8string.append(buf, is.gcount());
    }
  }
  
  return setFromUTF8String(utf8string);
}


//bool Text::setFromUTF8FileContents(const char* filename) {
//  std::ifstream ifs(filename, std::ifstream::in | std::ifstream::binary);
//  if (!ifs.good()) return false;
//  
//  ifs.seekg(0, std::ios::end);
//  size_t length = ifs.tellg();
//  ifs.seekg(0, std::ios::beg);
//
//  bool ok = setFromUTF8InputStream(ifs, length);
//  ifs.close();
//  return ok;
//}


bool Text::setFromUTF8FileOrSTDIN(const char* filename, std::string& error) {
  using namespace llvm;
  OwningPtr<MemoryBuffer> File;
  if (error_code ec = MemoryBuffer::getFileOrSTDIN(filename, File)) {
    error.assign(ec.message());
    return false;
  }
  if (!setFromUTF8Data((const uint8_t*)File->getBufferStart(), File->getBufferSize())) {
    error.assign("Input is not valid UTF-8 text");
    return false;
  }
  return true;
}


std::string Text::UTF8String() const {
  std::string utf8string;
  try {
    utf8::utf32to8(this->begin(), this->end(), std::back_inserter(utf8string));
  } catch (const utf8::invalid_code_point &e) {
    utf8string.clear();
  }
  return utf8string;
}


ByteList Text::rawByteList() const {
  Text::const_iterator it = begin();
  ByteList bytes;
  
  for (; it != end(); ++ it) {
    UChar c = *it;
    if (c < 0x100) {
      bytes.push_back(static_cast<uint8_t>(c));
    } else if (c < 0x10000) {
      bytes.push_back(static_cast<uint8_t>(c >> 8));
      bytes.push_back(static_cast<uint8_t>(c & 0xff));
    } else if (c < 0x1000000) {
      bytes.push_back(static_cast<uint8_t>(c >> 16));
      bytes.push_back(static_cast<uint8_t>(c >> 8 & 0xff));
      bytes.push_back(static_cast<uint8_t>(c & 0xff));
    } else if (c < 0x1000000) {
      bytes.push_back(static_cast<uint8_t>(c >> 24));
      bytes.push_back(static_cast<uint8_t>(c >> 16 & 0xff));
      bytes.push_back(static_cast<uint8_t>(c >> 8 & 0xff));
      bytes.push_back(static_cast<uint8_t>(c & 0xff));
    }
  }
  
  return bytes;
}


ByteString Text::rawByteString() const {
  Text::const_iterator it = begin();
  ByteString bytes;
  
  for (; it != end(); ++ it) {
    UChar c = *it;
    if (c < 0x100) {
      bytes.append(1, static_cast<uint8_t>(c));
    } else if (c < 0x10000) {
      bytes.append(1, static_cast<uint8_t>(c >> 8));
      bytes.append(1, static_cast<uint8_t>(c & 0xff));
    } else if (c < 0x1000000) {
      bytes.append(1, static_cast<uint8_t>(c >> 16));
      bytes.append(1, static_cast<uint8_t>(c >> 8 & 0xff));
      bytes.append(1, static_cast<uint8_t>(c & 0xff));
    } else if (c < 0x1000000) {
      bytes.append(1, static_cast<uint8_t>(c >> 24));
      bytes.append(1, static_cast<uint8_t>(c >> 16 & 0xff));
      bytes.append(1, static_cast<uint8_t>(c >> 8 & 0xff));
      bytes.append(1, static_cast<uint8_t>(c & 0xff));
    }
  }
  
  return bytes;
}


// static
std::string Text::UCharToUTF8String(const UChar c) {
  std::string utf8string;
  try {
    utf8::append(c, std::back_inserter(utf8string));
  } catch (const utf8::invalid_code_point &e) {
    utf8string.clear();
  }
  return utf8string;
}


std::vector<Text> Text::split(UChar separator) const {
  std::vector<Text> components;
  Text buf;

  // TODO: Optimize by avoiding copying of buf

  for (Text::const_iterator I = begin(), E = end(); I != E; ++I) {
    if (*I == separator) {
      if (!buf.empty()) {
        components.push_back(buf);
        buf.clear();
      }
    } else {
      buf.push_back(*I);
    }
  }

  if (!buf.empty()) {
    components.push_back(buf);
  }

  return components;
}


Text Text::join(const std::vector<Text>& components) const {
  if (components.size() == 0) return Text();
  else if (components.size() == 1) return components[0];

  Text text(components[0]);

  std::vector<Text>::const_iterator I = components.begin(), E = components.end();
  ++I; // components[0]
  for (;I != E; ++I) {
    text += *this;
    text += *I;
  }

  return text;
}


const UChar NullChar = 0;

} // namespace hue
