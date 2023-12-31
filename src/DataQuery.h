
#if defined(__AVR__)
 #include <ArduinoSTL.h>
#else
#include <cstdint>
#include <vector>
#endif

#ifndef DATAQUERY_H
#define DATAQUERY_H

typedef struct {
    String name;
    uint32_t    size;
} Field_t;

typedef struct {
    std::vector<String> record;
} Record_t;

class DataQuery_t {
    public:
        DataQuery_t() {;}
        ~DataQuery_t() {;}

        void clear() {
            if (this->fields.size() > 0){
                this->fields.clear();
            }
            if (this->records.size() > 0){
                this->records.clear();
            }
            this->fieldCount = 0;
            this->recordCount = 0;
        }

        const char* getRowValue(int row, const char* fieldName) {
            int index = 0;
            for (Field_t field : fields) {
                if (field.name.equals(fieldName)) {
                    if (records.at(row).record.at(index).length())
                        return records.at(row).record.at(index).c_str();
                    else
                        return "\0";
                }
                index++;
            }
            return nullptr;
        }

        const char* getRowValue(int row, int col) {
            if (row <= recordCount && col <= fieldCount) {
                if (records.at(row).record.at(col).length())
                    return records.at(row).record.at(col).c_str();
                else
                    return "\0";
            }
            return nullptr;
        }

        const char* getFieldName(int col) {
            int index = 0;
            for (Field_t field : fields) {
                if (index == col)
                    return field.name.c_str();
                index++;
            }
            return nullptr;
        }

        uint16_t fieldCount = 0;
        uint16_t recordCount = 0;
        std::vector<Field_t> fields;
        std::vector<Record_t> records;

        std::vector<Field_t>* getFields() {return &fields;}
        std::vector<Record_t>* getRecords() {return &records;}

};

#endif