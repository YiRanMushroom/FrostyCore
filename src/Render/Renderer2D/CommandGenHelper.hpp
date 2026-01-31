#pragma once

#define GenCommandEncoderBase(NameString)\
    public:\
    using EncoderType = CommandEncoder<NameString>;

#define GenPropertyWrite(PropName, ...) \
    EncoderType &Set##PropName(__VA_ARGS__ const &value) & { \
        this->PropName = value; \
        return *this; \
    }\
    \
    EncoderType &&Set##PropName(__VA_ARGS__ const &value) && { \
        this->PropName = value; \
        return std::move(*this); \
    }

#define EncoderProperty(PropName, ...) \
    __VA_ARGS__ PropName; \
    GenPropertyWrite(PropName, __VA_ARGS__)

#define EncoderPropertyOptional(PropName, ...) \
    std::optional<__VA_ARGS__> PropName; \
    GenPropertyWrite(PropName, __VA_ARGS__) // sets the value and removes the optional