/******************************************************************************
 * $Id: ogrgeojsonwriter.h 8075d2214b8fc3baf71cb7f1eb983415790dc7ef 2018-03-28 20:59:35 +0200 Even Rouault $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGR_GEOJSONWRITER_H_INCLUDED
#define OGR_GEOJSONWRITER_H_INCLUDED

#include <ogr_core.h>

#include "cpl_json_header.h"
#include "cpl_string.h"

/************************************************************************/
/*                         FORWARD DECLARATIONS                         */
/************************************************************************/
#ifdef __cplusplus
class OGRFeature;
class OGRGeometry;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;
#endif

CPL_C_START
/* %.XXXf formatting */
json_object CPL_DLL *json_object_new_double_with_precision(double dfVal, int nCoordPrecision);

/* %.XXXg formatting */
json_object CPL_DLL* json_object_new_double_with_significant_figures(double dfVal,
                                                                     int nSignificantFigures);
CPL_C_END

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/
#ifdef __cplusplus
class OGRCoordinateTransformation;

/*! @cond Doxygen_Suppress */
class OGRGeoJSONWriteOptions
{
    public:
        bool bWriteBBOX;
        bool bBBOXRFC7946;
        int  nCoordPrecision;
        int  nSignificantFigures;
        bool bPolygonRightHandRule;
        bool bCanPatchCoordinatesWithNativeData;
        bool bHonourReservedRFC7946Members;
        CPLString osIDField;
        bool bForceIDFieldType;
        OGRFieldType eForcedIDFieldType;

        OGRGeoJSONWriteOptions():
            bWriteBBOX(false),
            bBBOXRFC7946(false),
            nCoordPrecision(-1),
            nSignificantFigures(-1),
            bPolygonRightHandRule(false),
            bCanPatchCoordinatesWithNativeData(true),
            bHonourReservedRFC7946Members(false),
            bForceIDFieldType(false),
            eForcedIDFieldType(OFTString)
        {}

        void SetRFC7946Settings();
};
/*! @endcond */

OGREnvelope3D OGRGeoJSONGetBBox( OGRGeometry* poGeometry,
                                 const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteFeature( OGRFeature* poFeature, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteAttributes( OGRFeature* poFeature,
                                        bool bWriteIdIfFoundInAttributes = true,
                                        const OGRGeoJSONWriteOptions& oOptions = OGRGeoJSONWriteOptions() );
json_object* OGRGeoJSONWriteGeometry( const OGRGeometry* poGeometry, int nCoordPrecision, int nSignificantFigures );
json_object* OGRGeoJSONWriteGeometry( const OGRGeometry* poGeometry, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWritePoint( const OGRPoint* poPoint, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteLineString( const OGRLineString* poLine, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWritePolygon( const OGRPolygon* poPolygon, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteMultiPoint( const OGRMultiPoint* poGeometry, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteMultiLineString( const OGRMultiLineString* poGeometry, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteMultiPolygon( const OGRMultiPolygon* poGeometry, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteGeometryCollection( const OGRGeometryCollection* poGeometry, const OGRGeoJSONWriteOptions& oOptions );

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, double const& fZ, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteLineCoords( const OGRLineString* poLine, const OGRGeoJSONWriteOptions& oOptions );
json_object* OGRGeoJSONWriteRingCoords( const OGRLinearRing* poLine, bool bIsExteriorRing, const OGRGeoJSONWriteOptions& oOptions );
#endif

#endif /* OGR_GEOJSONWRITER_H_INCLUDED */
