#include "bitmapkit/internal.h"

typedef struct bk_tag_name {
  uint32_t value;
  const char *name;
  const char *group;
} bk_tag_name;

static const bk_tag_name tiff_tags[] = {
    {254u, "NewSubfileType", "TIFF"},
    {255u, "SubfileType", "TIFF"},
    {256u, "ImageWidth", "TIFF"},
    {257u, "ImageLength", "TIFF"},
    {258u, "BitsPerSample", "TIFF"},
    {259u, "Compression", "TIFF"},
    {262u, "PhotometricInterpretation", "TIFF"},
    {263u, "Threshholding", "TIFF"},
    {264u, "CellWidth", "TIFF"},
    {265u, "CellLength", "TIFF"},
    {266u, "FillOrder", "TIFF"},
    {269u, "DocumentName", "TIFF"},
    {270u, "ImageDescription", "TIFF"},
    {271u, "Make", "TIFF"},
    {272u, "Model", "TIFF"},
    {273u, "StripOffsets", "TIFF"},
    {274u, "Orientation", "TIFF"},
    {277u, "SamplesPerPixel", "TIFF"},
    {278u, "RowsPerStrip", "TIFF"},
    {279u, "StripByteCounts", "TIFF"},
    {282u, "XResolution", "TIFF"},
    {283u, "YResolution", "TIFF"},
    {284u, "PlanarConfiguration", "TIFF"},
    {285u, "PageName", "TIFF"},
    {286u, "XPosition", "TIFF"},
    {287u, "YPosition", "TIFF"},
    {288u, "FreeOffsets", "TIFF"},
    {289u, "FreeByteCounts", "TIFF"},
    {290u, "GrayResponseUnit", "TIFF"},
    {291u, "GrayResponseCurve", "TIFF"},
    {292u, "T4Options", "TIFF"},
    {293u, "T6Options", "TIFF"},
    {296u, "ResolutionUnit", "TIFF"},
    {297u, "PageNumber", "TIFF"},
    {301u, "TransferFunction", "TIFF"},
    {305u, "Software", "TIFF"},
    {306u, "DateTime", "TIFF"},
    {315u, "Artist", "TIFF"},
    {316u, "HostComputer", "TIFF"},
    {317u, "Predictor", "TIFF"},
    {318u, "WhitePoint", "TIFF"},
    {319u, "PrimaryChromaticities", "TIFF"},
    {320u, "ColorMap", "TIFF"},
    {321u, "HalftoneHints", "TIFF"},
    {322u, "TileWidth", "TIFF"},
    {323u, "TileLength", "TIFF"},
    {324u, "TileOffsets", "TIFF"},
    {325u, "TileByteCounts", "TIFF"},
    {330u, "SubIFDs", "TIFF"},
    {332u, "InkSet", "TIFF"},
    {333u, "InkNames", "TIFF"},
    {334u, "NumberOfInks", "TIFF"},
    {336u, "DotRange", "TIFF"},
    {337u, "TargetPrinter", "TIFF"},
    {338u, "ExtraSamples", "TIFF"},
    {339u, "SampleFormat", "TIFF"},
    {340u, "SMinSampleValue", "TIFF"},
    {341u, "SMaxSampleValue", "TIFF"},
    {342u, "TransferRange", "TIFF"},
    {343u, "ClipPath", "TIFF"},
    {344u, "XClipPathUnits", "TIFF"},
    {345u, "YClipPathUnits", "TIFF"},
    {346u, "Indexed", "TIFF"},
    {347u, "JPEGTables", "TIFF"},
    {512u, "JPEGProc", "TIFF"},
    {513u, "JPEGInterchangeFormat", "TIFF"},
    {514u, "JPEGInterchangeFormatLength", "TIFF"},
    {515u, "JPEGRestartInterval", "TIFF"},
    {517u, "JPEGLosslessPredictors", "TIFF"},
    {518u, "JPEGPointTransforms", "TIFF"},
    {519u, "JPEGQTables", "TIFF"},
    {520u, "JPEGDCTables", "TIFF"},
    {521u, "JPEGACTables", "TIFF"},
    {529u, "YCbCrCoefficients", "TIFF"},
    {530u, "YCbCrSubSampling", "TIFF"},
    {531u, "YCbCrPositioning", "TIFF"},
    {532u, "ReferenceBlackWhite", "TIFF"},
    {700u, "XMP", "TIFF"},
    {18246u, "Rating", "TIFF"},
    {33432u, "Copyright", "TIFF"},
    {33434u, "ExposureTime", "Exif"},
    {33437u, "FNumber", "Exif"},
    {34665u, "ExifIFD", "Exif"},
    {34850u, "ExposureProgram", "Exif"},
    {34853u, "GPSIFD", "Exif"},
    {34855u, "ISOSpeedRatings", "Exif"},
    {36864u, "ExifVersion", "Exif"},
    {36867u, "DateTimeOriginal", "Exif"},
    {36868u, "DateTimeDigitized", "Exif"},
    {37121u, "ComponentsConfiguration", "Exif"},
    {37122u, "CompressedBitsPerPixel", "Exif"},
    {37377u, "ShutterSpeedValue", "Exif"},
    {37378u, "ApertureValue", "Exif"},
    {37379u, "BrightnessValue", "Exif"},
    {37380u, "ExposureBiasValue", "Exif"},
    {37381u, "MaxApertureValue", "Exif"},
    {37382u, "SubjectDistance", "Exif"},
    {37383u, "MeteringMode", "Exif"},
    {37384u, "LightSource", "Exif"},
    {37385u, "Flash", "Exif"},
    {37386u, "FocalLength", "Exif"},
    {37500u, "MakerNote", "Exif"},
    {37510u, "UserComment", "Exif"},
    {40960u, "FlashpixVersion", "Exif"},
    {40961u, "ColorSpace", "Exif"},
    {40962u, "PixelXDimension", "Exif"},
    {40963u, "PixelYDimension", "Exif"},
    {40965u, "InteropIFD", "Exif"},
    {41483u, "FlashEnergy", "Exif"},
    {41486u, "FocalPlaneXResolution", "Exif"},
    {41487u, "FocalPlaneYResolution", "Exif"},
    {41488u, "FocalPlaneResolutionUnit", "Exif"},
    {41495u, "SensingMethod", "Exif"},
    {41728u, "FileSource", "Exif"},
    {41729u, "SceneType", "Exif"},
    {41730u, "CFAPattern", "Exif"},
    {41985u, "CustomRendered", "Exif"},
    {41986u, "ExposureMode", "Exif"},
    {41987u, "WhiteBalance", "Exif"},
    {41988u, "DigitalZoomRatio", "Exif"},
    {41989u, "FocalLengthIn35mmFilm", "Exif"},
    {41990u, "SceneCaptureType", "Exif"},
    {41991u, "GainControl", "Exif"},
    {41992u, "Contrast", "Exif"},
    {41993u, "Saturation", "Exif"},
    {41994u, "Sharpness", "Exif"},
    {41995u, "DeviceSettingDescription", "Exif"},
    {41996u, "SubjectDistanceRange", "Exif"},
    {42016u, "ImageUniqueID", "Exif"},
};

const char *bk_tiff_tag_name(uint32_t tag) {
  size_t i;
  for (i = 0; i < sizeof(tiff_tags) / sizeof(tiff_tags[0]); ++i) {
    if (tiff_tags[i].value == tag)
      return tiff_tags[i].name;
  }
  return "UnknownTIFFTag";
}

const char *bk_tiff_tag_group(uint32_t tag) {
  size_t i;
  for (i = 0; i < sizeof(tiff_tags) / sizeof(tiff_tags[0]); ++i) {
    if (tiff_tags[i].value == tag)
      return tiff_tags[i].group;
  }
  return "Private";
}

int bk_tiff_tag_is_offset(uint32_t tag) {
  switch (tag) {
  case 273u:
  case 288u:
  case 324u:
  case 330u:
  case 34665u:
  case 34853u:
  case 40965u:
  case 513u:
    return 1;
  default:
    return 0;
  }
}

int bk_tiff_tag_is_byte_count(uint32_t tag) {
  switch (tag) {
  case 279u:
  case 289u:
  case 325u:
  case 514u:
    return 1;
  default:
    return 0;
  }
}

typedef struct bk_png_chunk_name {
  char code[5];
  const char *purpose;
  uint8_t critical;
  uint8_t multiple_allowed;
} bk_png_chunk_name;

static const bk_png_chunk_name png_chunks[] = {
    {"IHDR", "PNG chunk IHDR", 1u, 0u}, {"PLTE", "PNG chunk PLTE", 1u, 0u},
    {"IDAT", "PNG chunk IDAT", 1u, 1u}, {"IEND", "PNG chunk IEND", 1u, 0u},
    {"tRNS", "PNG chunk tRNS", 0u, 0u}, {"gAMA", "PNG chunk gAMA", 0u, 0u},
    {"cHRM", "PNG chunk cHRM", 0u, 0u}, {"sRGB", "PNG chunk sRGB", 0u, 0u},
    {"iCCP", "PNG chunk iCCP", 0u, 0u}, {"tEXt", "PNG chunk tEXt", 0u, 1u},
    {"zTXt", "PNG chunk zTXt", 0u, 1u}, {"iTXt", "PNG chunk iTXt", 0u, 1u},
    {"bKGD", "PNG chunk bKGD", 0u, 0u}, {"pHYs", "PNG chunk pHYs", 0u, 0u},
    {"sBIT", "PNG chunk sBIT", 0u, 0u}, {"sPLT", "PNG chunk sPLT", 0u, 1u},
    {"hIST", "PNG chunk hIST", 0u, 0u}, {"tIME", "PNG chunk tIME", 0u, 0u},
    {"acTL", "PNG chunk acTL", 0u, 0u}, {"fcTL", "PNG chunk fcTL", 0u, 1u},
    {"fdAT", "PNG chunk fdAT", 0u, 1u}, {"eXIf", "PNG chunk eXIf", 0u, 0u},
    {"oFFs", "PNG chunk oFFs", 0u, 0u}, {"pCAL", "PNG chunk pCAL", 0u, 0u},
    {"sCAL", "PNG chunk sCAL", 0u, 0u}, {"gIFg", "PNG chunk gIFg", 0u, 0u},
    {"gIFx", "PNG chunk gIFx", 0u, 0u}, {"vpAg", "PNG chunk vpAg", 0u, 0u},
};

const char *bk_png_chunk_purpose(const char code[4]) {
  size_t i;
  for (i = 0; i < sizeof(png_chunks) / sizeof(png_chunks[0]); ++i) {
    if (memcmp(png_chunks[i].code, code, 4) == 0)
      return png_chunks[i].purpose;
  }
  return "Unknown PNG chunk";
}

int bk_png_chunk_is_critical(const char code[4]) {
  size_t i;
  for (i = 0; i < sizeof(png_chunks) / sizeof(png_chunks[0]); ++i) {
    if (memcmp(png_chunks[i].code, code, 4) == 0)
      return png_chunks[i].critical != 0;
  }
  return code && code[0] >= 'A' && code[0] <= 'Z';
}

int bk_png_chunk_allows_multiple(const char code[4]) {
  size_t i;
  for (i = 0; i < sizeof(png_chunks) / sizeof(png_chunks[0]); ++i) {
    if (memcmp(png_chunks[i].code, code, 4) == 0)
      return png_chunks[i].multiple_allowed != 0;
  }
  return 1;
}
