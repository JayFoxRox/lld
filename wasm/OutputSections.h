//===- OutputSections.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_WASM_OUTPUT_SECTIONS_H
#define LLD_WASM_OUTPUT_SECTIONS_H

#include "InputChunks.h"
#include "WriterUtils.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/LLVM.h"
#include "llvm/ADT/DenseMap.h"

namespace lld {

namespace wasm {
class OutputSection;
}
std::string toString(const wasm::OutputSection &Section);

namespace wasm {

class OutputSegment;

class OutputSection {
public:
  OutputSection(uint32_t Type, std::string Name = "")
      : Type(Type), Name(Name) {}
  virtual ~OutputSection() = default;

  StringRef getSectionName() const;
  void setOffset(size_t NewOffset) {
    log("setOffset: " + toString(*this) + ": " + Twine(NewOffset));
    Offset = NewOffset;
  }
  void createHeader(size_t BodySize);
  virtual size_t getSize() const = 0;
  virtual void writeTo(uint8_t *Buf) = 0;
  virtual void finalizeContents() = 0;
  virtual uint32_t numRelocations() const { return 0; }
  virtual void writeRelocations(raw_ostream &OS) const {}

  std::string Header;
  uint32_t Type;
  std::string Name;

protected:
  size_t Offset = 0;
};

class SyntheticSection : public OutputSection {
public:
  SyntheticSection(uint32_t Type, std::string Name = "")
      : OutputSection(Type, Name), BodyOutputStream(Body) {
    if (!Name.empty())
      writeStr(BodyOutputStream, Name, "section name");
  }

  void writeTo(uint8_t *Buf) override {
    assert(Offset);
    log("writing " + toString(*this));
    memcpy(Buf + Offset, Header.data(), Header.size());
    memcpy(Buf + Offset + Header.size(), Body.data(), Body.size());
  }

  size_t getSize() const override { return Header.size() + Body.size(); }

  virtual void writeBody() {}

  void finalizeContents() override {
    writeBody();
    BodyOutputStream.flush();
    createHeader(Body.size());
  }

  raw_ostream &getStream() { return BodyOutputStream; }

  std::string Body;

protected:
  llvm::raw_string_ostream BodyOutputStream;
};

class CodeSection : public OutputSection {
public:
  explicit CodeSection(ArrayRef<InputFunction *> Functions)
      : OutputSection(llvm::wasm::WASM_SEC_CODE), Functions(Functions) {}

  size_t getSize() const override { assert(BodySize); return Header.size() + BodySize; }
  void writeTo(uint8_t *Buf) override;
  uint32_t numRelocations() const override;
  void writeRelocations(raw_ostream &OS) const override;
  void finalizeContents() override;

protected:
  ArrayRef<InputFunction *> Functions;
  std::string CodeSectionHeader;
  size_t BodySize = 0;
};

class DataSection : public OutputSection {
public:
  explicit DataSection(ArrayRef<OutputSegment *> Segments)
      : OutputSection(llvm::wasm::WASM_SEC_DATA), Segments(Segments) {}

  size_t getSize() const override { return Header.size() + BodySize; }
  void writeTo(uint8_t *Buf) override;
  uint32_t numRelocations() const override;
  void writeRelocations(raw_ostream &OS) const override;
  void finalizeContents() override;

protected:
  ArrayRef<OutputSegment *> Segments;
  std::string DataSectionHeader;
  size_t BodySize = 0;
};

// Represents a custom section in the output file.  Wasm custom sections are
// used for storing user-defined metadata.  Unlike the core sections types
// they are identified by their string name.
// The linker combines custom sections that have the same name by simply
// concatenating them.
// Note that some custom sections such as "name" and "linking" are handled
// separately and are instead synthesized by the linker.
class CustomSection : public OutputSection {
public:
  CustomSection(std::string Name, ArrayRef<InputSection *> InputSections)
      : OutputSection(llvm::wasm::WASM_SEC_CUSTOM, Name),
        InputSections(InputSections) {}
  size_t getSize() const override {
    return Header.size() + NameData.size() + PayloadSize;
  }
  void writeTo(uint8_t *Buf) override;
  uint32_t numRelocations() const override;
  void writeRelocations(raw_ostream &OS) const override;
  void finalizeContents() override;

protected:
  size_t PayloadSize = 0;
  ArrayRef<InputSection *> InputSections;
  std::string NameData;
};

class RelocSection : public SyntheticSection {
public:
  RelocSection(StringRef Name, OutputSection *Sec, uint32_t SectionIndex)
      : SyntheticSection(llvm::wasm::WASM_SEC_CUSTOM, Name), Sec(Sec),
        SectionIndex(SectionIndex) {}
  void writeBody() override;

protected:
  OutputSection* Sec;
  uint32_t SectionIndex;
};


} // namespace wasm
} // namespace lld

#endif // LLD_WASM_OUTPUT_SECTIONS_H
