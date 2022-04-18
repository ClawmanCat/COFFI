/*
Copyright (C) 2014-2014 by Serge Lamikhov-Center

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*! @file coffi_symbols.hpp
 * @brief COFFI library classes for the COFF symbols and symbol table.
 *
 * Do not include this file directly. This file is included by coffi.hpp.
 */

#ifndef COFFI_SYMBOLS_HPP
#define COFFI_SYMBOLS_HPP

#include <vector>

#include <coffi/coffi_utils.hpp>
#include <coffi/coffi_headers.hpp>

namespace COFFI {

//-------------------------------------------------------------------------
//! Class for accessing a COFF symbol.
template <typename Record> class symbol_tmpl
{
  public:
    //---------------------------------------------------------------------
    symbol_tmpl(string_to_name_provider* stn) : stn_{stn}
    {
        std::fill_n(reinterpret_cast<char*>(&header), sizeof(header), '\0');
    }

    //---------------------------------------------------------------------
    //! @accessors{symbol}
    COFFI_GET_SET_ACCESS(uint32_t, value);
    COFFI_GET_SET_ACCESS(uint16_t, section_number);
    COFFI_GET_SET_ACCESS(uint16_t, type);
    COFFI_GET_SET_ACCESS(uint8_t, storage_class);
    COFFI_GET_SET_ACCESS(uint8_t, aux_symbols_number);
    //! @endaccessors

    //---------------------------------------------------------------------
    uint32_t get_index() const { return index_; }

    //---------------------------------------------------------------------
    void set_index(uint32_t index) { index_ = index; }

    //------------------------------------------------------------------------------
    const std::string get_name() const
    {
        return stn_->string_to_name(header.name);
    }

    //---------------------------------------------------------------------
    void set_name(const std::string& value)
    {
        stn_->name_to_string(value, header.name);
    }

    //---------------------------------------------------------------------
    const std::vector<auxiliary_symbol_record>& get_auxiliary_symbols() const
    {
        return auxs;
    }

    //---------------------------------------------------------------------
    std::vector<auxiliary_symbol_record>& get_auxiliary_symbols()
    {
        return auxs;
    }

    //---------------------------------------------------------------------
    bool load(std::istream& stream)
    {
        const uint32_t aux_padding = sizeof(Record) - sizeof(auxiliary_symbol_record);

        stream.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (stream.gcount() != sizeof(header)) {
            return false;
        }

        for (uint8_t i = 0; i < get_aux_symbols_number(); ++i) {
            auxiliary_symbol_record a;

            auto startg = stream.tellg();
            stream.read(reinterpret_cast<char*>(&a), sizeof(a));
            stream.seekg(stream.tellg() + (std::streamoff) aux_padding);
            auto endg = stream.tellg();

            if ((endg - startg) != sizeof(Record)) {
                return false;
            }
            auxs.push_back(a);
        }
        return true;
    }

    //---------------------------------------------------------------------
    void save(std::ostream& stream)
    {
        // Padding may be zero but we cannot have a zero size array, so always add one extra byte.
        const char aux_padding[sizeof(Record) - sizeof(auxiliary_symbol_record) + 1] = { };

        set_aux_symbols_number(auxs.size());
        stream.write(reinterpret_cast<char*>(&header), sizeof(header));

        for (auto aux : auxs) {
            stream.write(reinterpret_cast<char*>(&aux), sizeof(aux));
            stream.write(aux_padding, sizeof(Record) - sizeof(auxiliary_symbol_record));
        }
    }

    //---------------------------------------------------------------------
    bool is_bigobj_record() const {
        return std::is_same_v<Record, big_symbol_record>;
    }

    //---------------------------------------------------------------------
    symbol_tmpl<big_symbol_record> widen_record() const {
        big_symbol_record new_header;
        memcpy(new_header.name, header.name, 8);
        new_header.value              = header.value;
        new_header.section_number     = (uint32_t) header.section_number;
        new_header.type               = header.type;
        new_header.storage_class      = header.storage_class;
        new_header.aux_symbols_number = header.aux_symbols_number;

        symbol_tmpl<big_symbol_record> result;
        result.header = new_header;
        result.auxs   = auxs;
        result.index_ = index_;
        result.stn_   = stn_;

        return result;
    }

    //---------------------------------------------------------------------
    symbol_tmpl<symbol_record> narrow_record() const {
        symbol_record new_header;
        memcpy(new_header.name, header.name, 8);
        new_header.value              = header.value;
        new_header.section_number     = (uint16_t) header.section_number;
        new_header.type               = header.type;
        new_header.storage_class      = header.storage_class;
        new_header.aux_symbols_number = header.aux_symbols_number;

        symbol_tmpl<symbol_record> result;
        result.header = new_header;
        result.auxs   = auxs;
        result.index_ = index_;
        result.stn_   = stn_;

        return result;
    }

  protected:
    Record                               header;
    std::vector<auxiliary_symbol_record> auxs;
    uint32_t                             index_;
    string_to_name_provider*             stn_;

  private:
    template <typename> friend class symbol_tmpl;
    symbol_tmpl() = default;
};


//-------------------------------------------------------------------------
//! Class for accessing the symbol table.
class coffi_symbols : public virtual symbol_provider,
                      public virtual string_to_name_provider
{
  public:
    //---------------------------------------------------------------------
    coffi_symbols() : is_bigobj(false) {}

    //---------------------------------------------------------------------
    ~coffi_symbols() { clean_symbols(); }

    //---------------------------------------------------------------------
    //! @copydoc symbol_provider::get_symbol(uint32_t)
    virtual symbol* get_symbol(uint32_t index)
    {
        return (symbol*)((const coffi_symbols*)this)->get_symbol(index);
    }

    //---------------------------------------------------------------------
    //! @copydoc symbol_provider::get_symbol(uint32_t)
    virtual const symbol* get_symbol(uint32_t index) const
    {
        uint32_t L = 0;
        uint32_t R = symbols_.size() - 1;
        while (L <= R) {
            uint32_t m = (L + R) / 2;
            if (symbols_[m].get_index() < index) {
                L = m + 1;
            }
            else if (symbols_[m].get_index() > index) {
                R = m - 1;
            }
            else {
                return &(symbols_[m]);
            }
        }
        return nullptr;
    }

    //---------------------------------------------------------------------
    //! @copydoc symbol_provider::get_symbol(const std::string &)
    virtual symbol* get_symbol(const std::string& name)
    {
        return (symbol*)((const coffi_symbols*)this)->get_symbol(name);
    }

    //---------------------------------------------------------------------
    //! @copydoc symbol_provider::get_symbol(const std::string &)
    virtual const symbol* get_symbol(const std::string& name) const
    {
        for (auto s = symbols_.begin(); s != symbols_.end(); s++) {
            if (s->get_name() == name) {
                return &(*s);
            }
        }
        return nullptr;
    }

    //---------------------------------------------------------------------
    std::vector<symbol>* get_symbols() { return &symbols_; }

    //---------------------------------------------------------------------
    const std::vector<symbol>* get_symbols() const { return &symbols_; }

    //---------------------------------------------------------------------
    //! @copydoc symbol_provider::add_symbol()
    symbol* add_symbol(const std::string& name)
    {
        uint32_t index = 0;
        if (symbols_.size() > 0) {
            index = (symbols_.end() - 1)->get_index() + 1 +
                    (symbols_.end() - 1)->get_auxiliary_symbols().size();
        }
        symbol s{this};
        s.set_index(index);
        s.set_name(name);
        symbols_.push_back(s);
        return &*(symbols_.end() - 1);
    }

    //---------------------------------------------------------------------
  protected:
    //---------------------------------------------------------------------
    void clean_symbols() { symbols_.clear(); }

    //---------------------------------------------------------------------
    template <typename Symbol> bool load_symbols_impl(std::istream& stream, const coff_header* header)
    {
        if (header->get_symbol_table_offset() == 0) {
            return true;
        }

        stream.seekg(header->get_symbol_table_offset());
        for (uint32_t i = 0; i < header->get_symbols_count(); ++i) {
            Symbol s{this};
            if (!s.load(stream)) {
                return false;
            }
            s.set_index(i);
            i += s.get_auxiliary_symbols().size();
            symbols_.push_back(s.widen_record());
        }

        return true;
    }

    bool load_symbols(std::istream& stream, const coff_header* header) {
        is_bigobj = header->get_is_bigobj();

        if (is_bigobj) {
            return load_symbols_impl<symbol>(stream, header);
        } else {
            return load_symbols_impl<narrow_symbol>(stream, header);
        }
    }

    //---------------------------------------------------------------------
    void save_symbols(std::ostream& stream)
    {
        if (is_bigobj) {
            for (auto s : symbols_) s.save(stream);
        } else {
            for (auto s : symbols_) s.narrow_record().save(stream);
        }
    }

    //---------------------------------------------------------------------
    uint32_t get_symbols_filesize()
    {
        uint32_t filesize = 0;
        for (auto s : symbols_) {
            filesize +=
                sizeof(big_symbol_record) * (1 + s.get_auxiliary_symbols().size());
        }
        return filesize;
    }

    //---------------------------------------------------------------------
    std::vector<symbol> symbols_;
    bool is_bigobj;
};

} // namespace COFFI

#endif //COFFI_SYMBOLS_HPP
