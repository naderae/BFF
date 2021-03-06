/******************************************************************************
 *
 * Project:  PDS 4 Driver; Planetary Data System Format
 * Purpose:  Implementation of PDS4Dataset
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Hobu Inc
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

#include "cpl_vsi_error.h"
#include "gdal_proxy.h"
#include "rawdataset.h"
#include "vrtdataset.h"
#include "ogr_spatialref.h"
#include "gdal_priv_templates.hpp"

#include <cstdlib>
#include <vector>
#include <algorithm>

extern "C" void GDALRegister_PDS4();

/************************************************************************/
/*                            PDS4Dataset                               */
/************************************************************************/

class PDS4Dataset final: public RawDataset
{
    friend class PDS4RawRasterBand;
    friend class PDS4WrapperRasterBand;

    VSILFILE       *m_fpImage;
    GDALDataset    *m_poExternalDS; // external dataset (GeoTIFF)
    CPLString       m_osWKT;
    bool            m_bGotTransform;
    double          m_adfGeoTransform[6];
    CPLString       m_osXMLFilename;
    CPLString       m_osImageFilename;

    // Write dedicated parameters
    bool            m_bMustInitImageFile;
    bool            m_bUseSrcLabel;
    bool            m_bWriteHeader;
    bool            m_bStripFileAreaObservationalFromTemplate;
    CPLString       m_osInterleave;
    char          **m_papszCreationOptions;
    CPLString       m_osXMLPDS4;

    void            WriteHeader();
    void            WriteGeoreferencing(CPLXMLNode* psCart);
    void            ReadGeoreferencing(CPLXMLNode* psProduct);
    bool            InitImageFile();

    void            SubstituteVariables(CPLXMLNode* psNode,
                                            char** papszDict);

public:
    PDS4Dataset();
    virtual ~PDS4Dataset();

    virtual int CloseDependentDatasets() override;

    virtual const char *GetProjectionRef() override;
    virtual CPLErr SetProjection(const char*) override;
    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;
    virtual char** GetFileList() override;
    virtual CPLErr SetMetadata( char** papszMD, const char* pszDomain = "" )
                                                                     override;

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions);
    static GDALDataset* CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                        PDS4RawRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class PDS4RawRasterBand final: public RawRasterBand
{
        friend class PDS4Dataset;

        bool      m_bHasOffset;
        bool      m_bHasScale;
        bool      m_bHasNoData;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

    public:
                 PDS4RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                     void * l_fpRaw,
                                     vsi_l_offset l_nImgOffset,
                                     int l_nPixelOffset,
                                     int l_nLineOffset,
                                     GDALDataType l_eDataType,
                                     int l_bNativeOrder,
                                     int l_bIsVSIL = FALSE,
                                     int l_bOwnsFP = FALSE );
        virtual ~PDS4RawRasterBand() {}

        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = nullptr ) override;
        virtual double GetScale( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        void    SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                         PDS4WrapperRasterBand                       */
/*                                                                      */
/*      proxy for bands stored in other formats.                        */
/* ==================================================================== */
/************************************************************************/
class PDS4WrapperRasterBand final: public GDALProxyRasterBand
{
        friend class PDS4Dataset;

        GDALRasterBand* m_poBaseBand;
        bool      m_bHasOffset;
        bool      m_bHasScale;
        bool      m_bHasNoData;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() override
                                                    { return m_poBaseBand; }

  public:
            explicit PDS4WrapperRasterBand( GDALRasterBand* poBaseBandIn );
            ~PDS4WrapperRasterBand() {}

        virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = nullptr ) override;
        virtual double GetScale( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        int             GetMaskFlags() override { return nMaskFlags; }
        GDALRasterBand* GetMaskBand() override { return poMask; }
        void            SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                             PDS4MaskBand                             */
/* ==================================================================== */

class PDS4MaskBand final: public GDALRasterBand
{
    GDALRasterBand  *m_poBaseBand;
    void            *m_pBuffer;
    std::vector<double> m_adfConstants;

  public:

                            PDS4MaskBand( GDALRasterBand* poBaseBand,
                                    const std::vector<double>& adfConstants);
                           ~PDS4MaskBand();

    virtual CPLErr          IReadBlock( int, int, void * ) override;

};

/************************************************************************/
/*                        PDS4WrapperRasterBand()                      */
/************************************************************************/

PDS4WrapperRasterBand::PDS4WrapperRasterBand( GDALRasterBand* poBaseBandIn ) :
    m_poBaseBand(poBaseBandIn),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_bHasNoData(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
    eDataType = m_poBaseBand->GetRasterDataType();
    m_poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void PDS4WrapperRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double PDS4WrapperRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double PDS4WrapperRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;

    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetOffset(dfNewOffset);

    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;

    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetScale(dfNewScale);

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PDS4WrapperRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasNoData;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    m_bHasNoData = true;

    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetNoDataValue(dfNewNoData);

    return CE_None;
}

/************************************************************************/
/*                               Fill()                                 */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::Fill(double dfRealValue, double dfImaginaryValue)
{
    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_bMustInitImageFile )
    {
        if( !poGDS->InitImageFile() )
            return CE_Failure;
    }
    return GDALProxyRasterBand::Fill( dfRealValue, dfImaginaryValue );
}


/************************************************************************/
/*                             IWriteBlock()                             */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                            void *pImage )

{
    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_bMustInitImageFile )
    {
        if( !poGDS->InitImageFile() )
            return CE_Failure;
    }
    return GDALProxyRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PDS4WrapperRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( eRWFlag == GF_Write && poGDS->m_bMustInitImageFile )
    {
        if( !poGDS->InitImageFile() )
            return CE_Failure;
    }
    return GDALProxyRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                       PDS4RawRasterBand()                            */
/************************************************************************/

PDS4RawRasterBand::PDS4RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                        void * l_fpRaw,
                                        vsi_l_offset l_nImgOffset,
                                        int l_nPixelOffset,
                                        int l_nLineOffset,
                                        GDALDataType l_eDataType,
                                        int l_bNativeOrder,
                                        int l_bIsVSIL, int l_bOwnsFP )
    : RawRasterBand(l_poDS, l_nBand, l_fpRaw, l_nImgOffset, l_nPixelOffset,
                    l_nLineOffset,
                    l_eDataType, l_bNativeOrder, l_bIsVSIL, l_bOwnsFP),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_bHasNoData(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void PDS4RawRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double PDS4RawRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double PDS4RawRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr PDS4RawRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr PDS4RawRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PDS4RawRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasNoData;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PDS4RawRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    m_bHasNoData = true;
    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PDS4RawRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                        void *pImage )

{
    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( poGDS->m_bMustInitImageFile )
    {
        if( !poGDS->InitImageFile() )
            return CE_Failure;
    }

    return RawRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PDS4RawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    PDS4Dataset* poGDS = reinterpret_cast<PDS4Dataset*>(poDS);
    if( eRWFlag == GF_Write && poGDS->m_bMustInitImageFile )
    {
        if( !poGDS->InitImageFile() )
            return CE_Failure;
    }

    return RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                            PDS4MaskBand()                            */
/************************************************************************/

PDS4MaskBand::PDS4MaskBand( GDALRasterBand* poBaseBand,
                            const std::vector<double>& adfConstants )
    : m_poBaseBand(poBaseBand)
    , m_pBuffer(nullptr)
    , m_adfConstants(adfConstants)
{
    eDataType = GDT_Byte;
    poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    nRasterXSize = poBaseBand->GetXSize();
    nRasterYSize = poBaseBand->GetYSize();
}

/************************************************************************/
/*                           ~PDS4MaskBand()                            */
/************************************************************************/

PDS4MaskBand::~PDS4MaskBand()
{
    VSIFree(m_pBuffer);
}

/************************************************************************/
/*                             FillMask()                               */
/************************************************************************/

template<class T>
static void FillMask      (void* pvBuffer,
                           GByte* pabyDst,
                           int nReqXSize, int nReqYSize,
                           int nBlockXSize,
                           const std::vector<double>& adfConstants)
{
    const T* pSrc = static_cast<T*>(pvBuffer);
    std::vector<T> aConstants;
    for(size_t i = 0; i < adfConstants.size(); i++ )
    {
        T cst;
        GDALCopyWord(adfConstants[i], cst);
        aConstants.push_back(cst);
    }

    for( int y = 0; y < nReqYSize; y++ )
    {
        for( int x = 0; x < nReqXSize; x++ )
        {
            const T nSrc = pSrc[y * nBlockXSize + x];
            if( std::find(aConstants.begin(), aConstants.end(), nSrc) !=
                                                        aConstants.end() )
            {
                pabyDst[y * nBlockXSize + x] = 0;
            }
            else
            {
                pabyDst[y * nBlockXSize + x] = 255;
            }
        }
    }
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr PDS4MaskBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    const GDALDataType eSrcDT = m_poBaseBand->GetRasterDataType();
    const int nSrcDTSize = GDALGetDataTypeSizeBytes(eSrcDT);
    if( m_pBuffer == nullptr )
    {
        m_pBuffer = VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, nSrcDTSize);
        if( m_pBuffer == nullptr )
            return CE_Failure;
    }

    int nXOff = nXBlock * nBlockXSize;
    int nReqXSize = nBlockXSize;
    if( nXOff + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nXOff;
    int nYOff = nYBlock * nBlockYSize;
    int nReqYSize = nBlockYSize;
    if( nYOff + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nYOff;

    if( m_poBaseBand->RasterIO( GF_Read,
                                nXOff, nYOff, nReqXSize, nReqYSize,
                                m_pBuffer,
                                nReqXSize, nReqYSize,
                                eSrcDT,
                                nSrcDTSize,
                                nSrcDTSize * nBlockXSize,
                                nullptr ) != CE_None )
    {
        return CE_Failure;
    }

    GByte* pabyDst = static_cast<GByte*>(pImage);
    if( eSrcDT == GDT_Byte )
    {
        FillMask<GByte>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        m_adfConstants);
    }
    else if( eSrcDT == GDT_UInt16 )
    {
        FillMask<GUInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                          m_adfConstants);
    }
    else if( eSrcDT == GDT_Int16 )
    {
        FillMask<GInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                         m_adfConstants);
    }
    else if( eSrcDT == GDT_UInt32 )
    {
        FillMask<GUInt32>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                          m_adfConstants);
    }
    else if( eSrcDT == GDT_Int32 )
    {
        FillMask<GInt32>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                         m_adfConstants);
    }
    else if( eSrcDT == GDT_Float32 )
    {
        FillMask<float>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        m_adfConstants);
    }
    else if( eSrcDT == GDT_Float64 )
    {
        FillMask<double>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                         m_adfConstants);
    }

    return CE_None;
}


/************************************************************************/
/*                            PDS4Dataset()                             */
/************************************************************************/

PDS4Dataset::PDS4Dataset() :
    m_fpImage(nullptr),
    m_poExternalDS(nullptr),
    m_bGotTransform(false),
    m_bMustInitImageFile(false),
    m_bUseSrcLabel(true),
    m_bWriteHeader(false),
    m_bStripFileAreaObservationalFromTemplate(false),
    m_papszCreationOptions(nullptr)
{
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}


/************************************************************************/
/*                           ~PDS4Dataset()                             */
/************************************************************************/

PDS4Dataset::~PDS4Dataset()
{
    if( m_bMustInitImageFile)
        CPL_IGNORE_RET_VAL(InitImageFile());
    PDS4Dataset::FlushCache();
    if( m_bWriteHeader )
        WriteHeader();
    if( m_fpImage )
        VSIFCloseL(m_fpImage);
    CSLDestroy(m_papszCreationOptions);
    PDS4Dataset::CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int PDS4Dataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( m_poExternalDS )
    {
        bHasDroppedRef = FALSE;
        delete m_poExternalDS;
        m_poExternalDS = nullptr;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PDS4Dataset::GetProjectionRef()

{
    if( !m_osWKT.empty() )
        return m_osWKT;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr PDS4Dataset::SetProjection(const char* pszWKT)

{
    if( eAccess == GA_ReadOnly )
        return CE_Failure;
    m_osWKT = pszWKT;
    if( m_poExternalDS )
        m_poExternalDS->SetProjection(pszWKT);
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PDS4Dataset::GetGeoTransform( double * padfTransform )

{
    if( m_bGotTransform )
    {
        memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PDS4Dataset::SetGeoTransform( double * padfTransform )

{
    if( padfTransform[1] <= 0.0 ||
        padfTransform[2] != 0.0 ||
        padfTransform[4] != 0.0 ||
        padfTransform[5] >= 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up geotransform supported");
        return CE_Failure;
    }
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double) * 6 );
    m_bGotTransform = true;
    if( m_poExternalDS )
        m_poExternalDS->SetGeoTransform(padfTransform);
    return CE_None;
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr PDS4Dataset::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( m_bUseSrcLabel && eAccess == GA_Update && pszDomain != nullptr &&
        EQUAL( pszDomain, "xml:PDS4" ) )
    {
        if( papszMD != nullptr && papszMD[0] != nullptr )
        {
            m_osXMLPDS4 = papszMD[0];
        }
        return CE_None;
    }
    return GDALPamDataset::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** PDS4Dataset::GetFileList()
{
    char** papszFileList = GDALPamDataset::GetFileList();
    if( !m_osXMLFilename.empty() &&
        CSLFindString(papszFileList, m_osXMLFilename) < 0 )
    {
        papszFileList = CSLAddString(papszFileList, m_osXMLFilename);
    }
    if(  !m_osImageFilename.empty() )
    {
        papszFileList = CSLAddString(papszFileList, m_osImageFilename);
    }
    return papszFileList;
}


/************************************************************************/
/*                               Identify()                             */
/************************************************************************/

int PDS4Dataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "PDS4:") )
        return TRUE;

    return poOpenInfo->nHeaderBytes > 0 &&
           strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                  "Product_Observational") != nullptr &&
           strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                  "http://pds.nasa.gov/pds4/pds/v1") != nullptr;
}

/************************************************************************/
/*                            GetLinearValue()                          */
/************************************************************************/

static const struct
{
    const char* pszUnit;
    double      dfToMeter;
} apsLinearUnits[] = {
    { "AU", 149597870700.0 },
    { "Angstrom", 1e-10 },
    { "cm", 1e-2 },
    { "km", 1e3 },
    { "micrometer", 1e-6 },
    { "mm", 1e-3 },
    { "nm", 1e-9 }
};

static double GetLinearValue(CPLXMLNode* psParent, const char* pszElementName)
{
    CPLXMLNode* psNode = CPLGetXMLNode(psParent, pszElementName);
    if( psNode == nullptr )
        return 0.0;
    double dfVal = CPLAtof(CPLGetXMLValue(psNode, nullptr, ""));
    const char* pszUnit = CPLGetXMLValue(psNode, "unit", nullptr);
    if( pszUnit && !EQUAL(pszUnit, "m") )
    {
        bool bFound = false;
        for( size_t i = 0; i < CPL_ARRAYSIZE(apsLinearUnits); i++ )
        {
            if( EQUAL(pszUnit, apsLinearUnits[i].pszUnit) )
            {
                dfVal *= apsLinearUnits[i].dfToMeter;
                bFound = true;
                break;
            }
        }
        if( !bFound )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unknown unit '%s' for '%s'",
                     pszUnit, pszElementName);
        }
    }
    return dfVal;
}

/************************************************************************/
/*                          GetResolutionValue()                        */
/************************************************************************/

static const struct
{
    const char* pszUnit;
    double      dfToMeter;
} apsResolutionUnits[] = {
    { "km/pixel", 1e3 },
    { "mm/pixel", 1e-3 },
};

static double GetResolutionValue(CPLXMLNode* psParent, const char* pszElementName)
{
    CPLXMLNode* psNode = CPLGetXMLNode(psParent, pszElementName);
    if( psNode == nullptr )
        return 0.0;
    double dfVal = CPLAtof(CPLGetXMLValue(psNode, nullptr, ""));
    const char* pszUnit = CPLGetXMLValue(psNode, "unit", nullptr);
    if( pszUnit && !EQUAL(pszUnit, "m/pixel") )
    {
        bool bFound = false;
        for( size_t i = 0; i < CPL_ARRAYSIZE(apsResolutionUnits); i++ )
        {
            if( EQUAL(pszUnit, apsResolutionUnits[i].pszUnit) )
            {
                dfVal *= apsResolutionUnits[i].dfToMeter;
                bFound = true;
                break;
            }
        }
        if( !bFound )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unknown unit '%s' for '%s'",
                     pszUnit, pszElementName);
        }
    }
    return dfVal;
}

/************************************************************************/
/*                            GetAngularValue()                         */
/************************************************************************/

static const struct
{
    const char* pszUnit;
    double      dfToDeg;
} apsAngularUnits[] = {
    { "arcmin", 1. / 60. },
    { "arcsec", 1. / 3600 },
    { "hr", 15.0 },
    { "mrad", 180.0 / M_PI / 1000. },
    { "rad", 180.0 / M_PI }
};

static double GetAngularValue(CPLXMLNode* psParent, const char* pszElementName,
                              bool* pbGotVal = nullptr)
{
    CPLXMLNode* psNode = CPLGetXMLNode(psParent, pszElementName);
    if( psNode == nullptr )
    {
        if( pbGotVal )
            *pbGotVal = false;
        return 0.0;
    }
    double dfVal = CPLAtof(CPLGetXMLValue(psNode, nullptr, ""));
    const char* pszUnit = CPLGetXMLValue(psNode, "unit", nullptr);
    if( pszUnit && !EQUAL(pszUnit, "deg") )
    {
        bool bFound = false;
        for( size_t i = 0; i < CPL_ARRAYSIZE(apsAngularUnits); i++ )
        {
            if( EQUAL(pszUnit, apsAngularUnits[i].pszUnit) )
            {
                dfVal *= apsAngularUnits[i].dfToDeg;
                bFound = true;
                break;
            }
        }
        if( !bFound )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unknown unit '%s' for '%s'",
                     pszUnit, pszElementName);
        }
    }
    if( pbGotVal )
        *pbGotVal = true;
    return dfVal;
}

/************************************************************************/
/*                          ReadGeoreferencing()                       */
/************************************************************************/

// See https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd
// and https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.sch
void PDS4Dataset::ReadGeoreferencing(CPLXMLNode* psProduct)
{
    CPLXMLNode* psCart = CPLGetXMLNode(psProduct,
                                       "Observation_Area.Discipline_Area.Cartography");
    if( psCart == nullptr )
    {
        CPLDebug("PDS4", "Did not find Observation_Area.Discipline_Area.Cartography");
        return;
    }

    // Bounding box: informative only
    CPLXMLNode* psBounding = CPLGetXMLNode(psCart,
                                    "Spatial_Domain.Bounding_Coordinates");
    if( psBounding )
    {
        const char* pszWest =
            CPLGetXMLValue(psBounding, "west_bounding_coordinate", nullptr);
        const char* pszEast =
            CPLGetXMLValue(psBounding, "east_bounding_coordinate", nullptr);
        const char* pszNorth =
            CPLGetXMLValue(psBounding, "north_bounding_coordinate", nullptr);
        const char* pszSouth =
            CPLGetXMLValue(psBounding, "south_bounding_coordinate", nullptr);
        if( pszWest )
            CPLDebug("PDS4", "West: %s", pszWest);
        if( pszEast )
            CPLDebug("PDS4", "East: %s", pszEast);
        if( pszNorth )
            CPLDebug("PDS4", "North: %s", pszNorth);
        if( pszSouth )
            CPLDebug("PDS4", "South: %s", pszSouth);
    }

    CPLXMLNode* psSR = CPLGetXMLNode(psCart,
        "Spatial_Reference_Information.Horizontal_Coordinate_System_Definition");
    if( psSR == nullptr )
    {
        CPLDebug("PDS4", "Did not find Spatial_Reference_Information."
                 "Horizontal_Coordinate_System_Definition");
        return;
    }

    OGRSpatialReference oSRS;
    CPLXMLNode* psGridCoordinateSystem = CPLGetXMLNode(psSR,
                                            "Planar.Grid_Coordinate_System");
    CPLXMLNode* psMapProjection = CPLGetXMLNode(psSR, "Planar.Map_Projection");
    CPLString osProjName;
    double dfCenterLon = 0.0;
    double dfCenterLat = 0.0;
    double dfStdParallel1 = 0.0;
    double dfStdParallel2 = 0.0;
    double dfScale = 1.0;
    if( psGridCoordinateSystem != nullptr )
    {
        osProjName = CPLGetXMLValue(psGridCoordinateSystem,
                                        "grid_coordinate_system_name", "");
        if( !osProjName.empty() )
        {
            if( osProjName == "Universal Transverse Mercator" )
            {
                CPLXMLNode* psUTMZoneNumber = CPLGetXMLNode(
                    psGridCoordinateSystem,
                    "Universal_Transverse_Mercator.utm_zone_number");
                if( psUTMZoneNumber )
                {
                    int nZone = atoi(CPLGetXMLValue(psUTMZoneNumber, nullptr, ""));
                    oSRS.SetUTM( std::abs(nZone), nZone >= 0 );
                }
            }
            else if( osProjName == "Universal Polar Stereographic" )
            {
                CPLXMLNode* psProjParamNode = CPLGetXMLNode(
                    psGridCoordinateSystem,
                    "Universal_Polar_Stereographic.Polar_Stereographic");
                if( psProjParamNode )
                {
                    dfCenterLon = GetAngularValue(psProjParamNode,
                                    "longitude_of_central_meridian");
                    dfCenterLat = GetAngularValue(psProjParamNode,
                                    "latitude_of_projection_origin");
                    dfScale = CPLAtof(CPLGetXMLValue(psProjParamNode,
                                "scale_factor_at_projection_origin", "1"));
                    oSRS.SetPS(dfCenterLat, dfCenterLon, dfScale, 0, 0);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "grid_coordinate_system_name = %s not supported",
                         osProjName.c_str());
            }
        }
    }
    else if( psMapProjection != nullptr )
    {
        osProjName = CPLGetXMLValue(psMapProjection,
                                                 "map_projection_name", "");
        if( !osProjName.empty() )
        {
            CPLXMLNode* psProjParamNode = CPLGetXMLNode(psMapProjection,
                          CPLString(osProjName).replaceAll(' ','_').c_str());
            if( psProjParamNode == nullptr &&
                // typo in https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.sch
                EQUAL(osProjName, "Orothographic") ) 
            {
                psProjParamNode = CPLGetXMLNode(psMapProjection, "Orthographic");
            }
            bool bGotStdParallel1 = false;
            bool bGotStdParallel2 = false;
            bool bGotScale = false;
            if( psProjParamNode )
            {
                bool bGotCenterLon = false;
                dfCenterLon = GetAngularValue(psProjParamNode,
                                              "longitude_of_central_meridian",
                                              &bGotCenterLon);
                if( !bGotCenterLon )
                {
                    dfCenterLon = GetAngularValue(psProjParamNode,
                                    "straight_vertical_longitude_from_pole", 
                                    &bGotCenterLon);
                }
                dfCenterLat = GetAngularValue(psProjParamNode,
                                    "latitude_of_projection_origin");
                dfStdParallel1 = GetAngularValue(psProjParamNode,
                                    "standard_parallel_1", &bGotStdParallel1);
                dfStdParallel2 = GetAngularValue(psProjParamNode,
                                    "standard_parallel_2", &bGotStdParallel2);
                const char* pszScaleParam =
                    ( osProjName == "Transverse Mercator" ) ?
                        "scale_factor_at_central_meridian":
                        "scale_factor_at_projection_origin";
                const char* pszScaleVal =
                    CPLGetXMLValue(psProjParamNode, pszScaleParam, nullptr);
                bGotScale = pszScaleVal != nullptr;
                dfScale = ( pszScaleVal ) ? CPLAtof(pszScaleVal) : 1.0;
            }

            CPLXMLNode* psObliqueAzimuth = CPLGetXMLNode(psProjParamNode,
                                                    "Oblique_Line_Azimuth");
            CPLXMLNode* psObliquePoint = CPLGetXMLNode(psProjParamNode,
                                                    "Oblique_Line_Point");

            if( EQUAL(osProjName, "Equirectangular") )
            {
                oSRS.SetEquirectangular2 (dfCenterLat, dfCenterLon,
                                          dfStdParallel1,
                                          0, 0);
            }
            else if( EQUAL(osProjName, "Lambert Conformal Conic") )
            {
                if( bGotScale )
                {
                    if( (bGotStdParallel1 && dfStdParallel1 != dfCenterLat) ||
                        (bGotStdParallel2 && dfStdParallel2 != dfCenterLat) )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Ignoring standard_parallel_1 and/or "
                                 "standard_parallel_2 with LCC_1SP formulation");
                    }
                    oSRS.SetLCC1SP(dfCenterLat, dfCenterLon,
                                   dfScale, 0, 0);
                }
                else
                {
                    oSRS.SetLCC(dfStdParallel1, dfStdParallel2,
                                dfCenterLat, dfCenterLon, 0, 0);
                }
            }
            else if( EQUAL(osProjName, "Oblique Mercator") &&
                     (psObliqueAzimuth != nullptr || psObliquePoint != nullptr) )
            {
                if( psObliqueAzimuth )
                {
                    // Not sure of this
                    dfCenterLon = CPLAtof(CPLGetXMLValue(psObliqueAzimuth,
                                    "azimuth_measure_point_longitude", "0"));

                    double dfAzimuth = CPLAtof(CPLGetXMLValue(psObliqueAzimuth,
                                            "azimuthal_angle", "0"));
                    oSRS.SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER );
                    oSRS.SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
                    oSRS.SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLon );
                    oSRS.SetNormProjParm( SRS_PP_AZIMUTH, dfAzimuth );
                    //SetNormProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, dfRectToSkew );
                    oSRS.SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
                    oSRS.SetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 );
                    oSRS.SetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 );
                }
                else
                {
                    double dfLat1 = 0.0;
                    double dfLong1 = 0.0;
                    double dfLat2 = 0.0;
                    double dfLong2 = 0.0;
                    CPLXMLNode* psPoint = CPLGetXMLNode(psObliquePoint,
                                                        "Oblique_Line_Point_Group");
                    if( psPoint )
                    {
                        dfLat1 = CPLAtof(CPLGetXMLValue(psPoint,
                                            "oblique_line_latitude", "0.0"));
                        dfLong1 = CPLAtof(CPLGetXMLValue(psPoint,
                                            "oblique_line_longitude", "0.0"));
                        psPoint = psPoint->psNext;
                        if( psPoint && psPoint->eType == CXT_Element &&
                            EQUAL(psPoint->pszValue, "Oblique_Line_Point_Group") )
                        {
                            dfLat2 = CPLAtof(CPLGetXMLValue(psPoint,
                                            "oblique_line_latitude", "0.0"));
                            dfLong2 = CPLAtof(CPLGetXMLValue(psPoint,
                                                "oblique_line_longitude", "0.0"));
                        }
                    }
                    oSRS.SetHOM2PNO( dfCenterLat,
                                     dfLat1, dfLong1,
                                     dfLat2, dfLong2,
                                     dfScale,
                                     0.0, 0.0 );
                }
            }
            else if( EQUAL(osProjName, "Polar Stereographic") )
            {
                oSRS.SetPS(dfCenterLat, dfCenterLon, dfScale, 0, 0);
            }
            else if( EQUAL(osProjName, "Polyconic") )
            {
                oSRS.SetPolyconic(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Sinusoidal") )
            {
                oSRS.SetSinusoidal(dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Transverse Mercator") )
            {
                oSRS.SetTM(dfCenterLat, dfCenterLon, dfScale, 0, 0);
            }

            // Below values are valid map_projection_name according to
            // the schematron but they don't have a dedicated element to
            // hold the projection parameter. Assumed the schema is extended
            // similarly to the existing for a few obvious ones
            else if( EQUAL(osProjName, "Albers Conical Equal Area") )
            {
                oSRS.SetACEA( dfStdParallel1, dfStdParallel2,
                              dfCenterLat, dfCenterLon,
                              0.0, 0.0 );
            }
            else if( EQUAL(osProjName, "Azimuthal Equidistant") )
            {
                oSRS.SetAE(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Equidistant Conic") )
            {
                oSRS.SetEC( dfStdParallel1, dfStdParallel2,
                            dfCenterLat, dfCenterLon,
                            0.0, 0.0 );
            }
            // Unhandled: General Vertical Near-sided Projection
            else if( EQUAL(osProjName, "Gnomonic") )
            {
                oSRS.SetGnomonic(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Lambert Azimuthal Equal Area") )
            {
                oSRS.SetLAEA(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Miller Cylindrical") )
            {
                oSRS.SetMC(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Orothographic") // typo
                     || EQUAL(osProjName, "Orthographic") )
            {
                osProjName = "Orthographic";
                oSRS.SetOrthographic(dfCenterLat, dfCenterLon, 0, 0);
            }
            else if( EQUAL(osProjName, "Robinson") )
            {
                oSRS.SetRobinson(dfCenterLon, 0, 0);
            }
            // Unhandled: Space Oblique Mercator
            else if( EQUAL(osProjName, "Stereographic") )
            {
                oSRS.SetStereographic(dfCenterLat, dfCenterLon, dfScale, 0, 0);
            }
            else if( EQUAL(osProjName, "van der Grinten") )
            {
                oSRS.SetVDG(dfCenterLon, 0, 0);
            }

            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "map_projection_name = %s not supported",
                         osProjName.c_str());
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Planar.Map_Projection not found");
    }

    CPLXMLNode* psGeodeticModel = CPLGetXMLNode(psSR, "Geodetic_Model");
    if( psGeodeticModel != nullptr )
    {
        const char* pszLatitudeType = CPLGetXMLValue(psGeodeticModel,
                                                     "latitude_type",
                                                     "");
        bool bIsOgraphic = EQUAL(pszLatitudeType, "planetographic");

        double dfSemiMajor = GetLinearValue(psGeodeticModel,
                                            "semi_major_radius");
        // according to the spec, it seems the semi_minor_radius is
        // considered in the equatorial plane, which is rather unusual
        // for WKT, we want to use the polar_radius as the actual semi minor
        // axis
        double dfSemiMinorPDS4 = GetLinearValue(psGeodeticModel,
                                            "semi_minor_radius");
        if( dfSemiMajor != dfSemiMinorPDS4 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "semi_minor_radius = %f m, different from "
                     "semi_major_radius = %f, will be ignored",
                     dfSemiMinorPDS4, dfSemiMajor);
        }
        double dfPolarRadius = GetLinearValue(psGeodeticModel,
                                              "polar_radius");
        // Use the polar_radius as the actual semi minor
        const double dfSemiMinor = dfPolarRadius;

        // Compulsory
        const char* pszTargetName = CPLGetXMLValue(psProduct,
            "Observation_Area.Target_Identification.name", "unknown");

        CPLString osProjTargetName = osProjName + " " + pszTargetName;
        oSRS.SetProjCS(osProjTargetName);

        CPLString osGeogName = CPLString("GCS_") + pszTargetName;

        CPLString osSphereName = CPLGetXMLValue(psGeodeticModel,
                                                "spheroid_name",
                                                pszTargetName);
        CPLString osDatumName  = "D_" + osSphereName;

        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double dfInvFlattening = 0;
        if((dfSemiMajor - dfSemiMinor) >= 0.00000001)
        {
            dfInvFlattening = dfSemiMajor / (dfSemiMajor - dfSemiMinor);
        }

        //(if stereographic with center lat ==90) or (polar stereographic )
        if ( ( EQUAL(osProjName, "STEREOGRAPHIC" ) && fabs(dfCenterLat) == 90) ||
            (EQUAL(osProjName, "POLAR STEREOGRAPHIC") )){
            if (bIsOgraphic)
            {
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName, dfSemiMajor,
                                dfInvFlattening, "Reference_Meridian", 0.0);
            }
            else
            {
                osSphereName += "_polarRadius";
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                dfPolarRadius, 0.0, "Reference_Meridian", 0.0);
            }
        }
        else if((EQUAL(osProjName, "EQUIRECTANGULAR")) ||
                (EQUAL(osProjName, "ORTHOGRAPHIC")) ||
                (EQUAL(osProjName, "STEREOGRAPHIC")) ||
                (EQUAL(osProjName, "SINUSOIDAL")) ){
            oSRS.SetGeogCS(osGeogName, osDatumName, osSphereName,
                            dfSemiMajor, 0.0, "Reference_Meridian", 0.0);
        }
        else
        {
            if(bIsOgraphic)
            {
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                dfSemiMajor, dfInvFlattening, "Reference_Meridian", 0.0);
            }
            else
            {
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                dfSemiMajor, 0.0, "Reference_Meridian", 0.0);
            }
        }

    }

    CPLXMLNode* psPCI = CPLGetXMLNode(psSR, "Planar.Planar_Coordinate_Information");
    CPLXMLNode* psGT = CPLGetXMLNode(psSR, "Planar.Geo_Transformation");
    if( psPCI && psGT )
    {
        const char* pszPCIEncoding = CPLGetXMLValue(psPCI,
                                    "planar_coordinate_encoding_method", "");
        CPLXMLNode* psCR = CPLGetXMLNode(psPCI, "Coordinate_Representation");
        if( !EQUAL(pszPCIEncoding, "Coordinate Pair") )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "planar_coordinate_encoding_method = %s not supported",
                     pszPCIEncoding);
        }
        else if( psCR != nullptr )
        {
            double dfXRes = GetResolutionValue(psCR, "pixel_resolution_x");
            double dfYRes = GetResolutionValue(psCR, "pixel_resolution_y");
            double dfULX = GetLinearValue(psGT, "upperleft_corner_x");
            double dfULY = GetLinearValue(psGT, "upperleft_corner_y");
            // Correcting from pixel center convention to pixel corner
            // convention
            m_adfGeoTransform[0] = dfULX - 0.5 * dfXRes;
            m_adfGeoTransform[1] = dfXRes;
            m_adfGeoTransform[2] = 0.0;
            m_adfGeoTransform[3] = dfULY + 0.5 * dfYRes;
            m_adfGeoTransform[4] = 0.0;
            m_adfGeoTransform[5] = -dfYRes;
            m_bGotTransform = true;
        }
    }

    char* pszWKT = nullptr;
    oSRS.exportToWkt(&pszWKT);
    if( pszWKT )
        m_osWKT = pszWKT;
    CPLFree(pszWKT);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

// See https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.xsd
// and https://pds.nasa.gov/pds4/pds/v1/PDS4_PDS_1800.sch
GDALDataset* PDS4Dataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    CPLString osXMLFilename(poOpenInfo->pszFilename);
    int nFAOIdxLookup = -1;
    int nArrayIdxLookup = -1;
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "PDS4:") )
    {
        char** papszTokens =
            CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        int nCount = CSLCount(papszTokens);
        if( nCount == 5 && strlen(papszTokens[1]) == 1 &&
            (papszTokens[2][0] == '\\' || papszTokens[2][0] == '/') )
        {
            osXMLFilename = CPLString(papszTokens[1]) + ":" + papszTokens[2];
            nFAOIdxLookup = atoi(papszTokens[3]);
            nArrayIdxLookup = atoi(papszTokens[4]);
        }
        else if( nCount == 5 &&
            (EQUAL(papszTokens[1], "/vsicurl/http") ||
             EQUAL(papszTokens[1], "/vsicurl/https")) )
        {
            osXMLFilename = CPLString(papszTokens[1]) + ":" + papszTokens[2];
            nFAOIdxLookup = atoi(papszTokens[3]);
            nArrayIdxLookup = atoi(papszTokens[4]);
        }
        else if( nCount == 4 )
        {
            osXMLFilename = papszTokens[1];
            nFAOIdxLookup = atoi(papszTokens[2]);
            nArrayIdxLookup = atoi(papszTokens[3]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid syntax for PDS4 subdataset name");
            CSLDestroy(papszTokens);
            return nullptr;
        }
        CSLDestroy(papszTokens);
    }

    CPLXMLNode* psRoot = CPLParseXMLFile(osXMLFilename);
    CPLXMLTreeCloser oCloser(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    CPLXMLNode* psProduct = CPLGetXMLNode(psRoot, "=Product_Observational");
    if( psProduct == nullptr )
        return nullptr;

    // Test case: https://starbase.jpl.nasa.gov/pds4/1700/dph_example_products/test_Images_DisplaySettings/TestPattern_Image/TestPattern.xml
    const char* pszVertDir = CPLGetXMLValue(psProduct,
        "Observation_Area.Discipline_Area.Display_Settings.Display_Direction."
        "vertical_display_direction", "");
    const bool bBottomToTop = EQUAL(pszVertDir, "Bottom to Top");

    PDS4Dataset *poDS = new PDS4Dataset();

    CPLStringList aosSubdatasets;
    int nFAOIdx = 0;
    for( CPLXMLNode* psIter = psProduct->psChild;
                     psIter != nullptr;
                     psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            strcmp(psIter->pszValue, "File_Area_Observational") != 0 )
        {
            continue;
        }

        nFAOIdx ++;

        CPLXMLNode* psFile = CPLGetXMLNode(psIter, "File");
        if( psFile == nullptr )
        {
            continue;
        }
        const char* pszFilename = CPLGetXMLValue(psFile, "file_name", nullptr);
        if( pszFilename == nullptr )
        {
            continue;
        }

        int nArrayIdx = 0;
        for( CPLXMLNode* psSubIter = psIter->psChild;
             (nFAOIdxLookup < 0 || nFAOIdxLookup == nFAOIdx) &&
             psSubIter != nullptr;
             psSubIter = psSubIter->psNext )
        {
            if( psSubIter->eType != CXT_Element )
            {
                continue;
            }
            int nDIM = 0;
            if( STARTS_WITH(psSubIter->pszValue, "Array_1D") )
            {
                nDIM = 1;
            }
            else if( STARTS_WITH(psSubIter->pszValue, "Array_2D") )
            {
                nDIM = 2;
            }
            else if ( STARTS_WITH(psSubIter->pszValue, "Array_3D") )
            {
                nDIM = 3;
            }
            else if( strcmp(psSubIter->pszValue, "Array") == 0 )
            {
                nDIM = atoi(CPLGetXMLValue(psSubIter, "axes", "0"));
            }
            if( nDIM == 0 )
            {
                continue;
            }

            nArrayIdx ++;
            // Does it match a selected subdataset ?
            if( nArrayIdxLookup > 0 && nArrayIdx != nArrayIdxLookup)
            {
                continue;
            }

            const char* pszArrayName = CPLGetXMLValue(psSubIter,
                                                      "name", nullptr);
            const char* pszArrayId = CPLGetXMLValue(psSubIter,
                                                    "local_identifier", nullptr);
            vsi_l_offset nOffset = static_cast<vsi_l_offset>(CPLAtoGIntBig(
                                CPLGetXMLValue(psSubIter, "offset", "0")));

            const char* pszAxisIndexOrder = CPLGetXMLValue(
                psSubIter, "axis_index_order", "");
            if( !EQUAL(pszAxisIndexOrder, "Last Index Fastest") )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "axis_index_order = '%s' unhandled",
                         pszAxisIndexOrder);
                continue;
            }

            // Figure out data type
            const char* pszDataType = CPLGetXMLValue(psSubIter,
                                        "Element_Array.data_type", "");
            GDALDataType eDT = GDT_Byte;
            bool bSignedByte = false;
            bool bLSBOrder = strstr(pszDataType, "LSB") != nullptr;

            // ComplexLSB16', 'ComplexLSB8', 'ComplexMSB16', 'ComplexMSB8', 'IEEE754LSBDouble', 'IEEE754LSBSingle', 'IEEE754MSBDouble', 'IEEE754MSBSingle', 'SignedBitString', 'SignedByte', 'SignedLSB2', 'SignedLSB4', 'SignedLSB8', 'SignedMSB2', 'SignedMSB4', 'SignedMSB8', 'UnsignedBitString', 'UnsignedByte', 'UnsignedLSB2', 'UnsignedLSB4', 'UnsignedLSB8', 'UnsignedMSB2', 'UnsignedMSB4', 'UnsignedMSB8'
            if( EQUAL(pszDataType, "ComplexLSB16") ||
                EQUAL(pszDataType, "ComplexMSB16") )
            {
                eDT = GDT_CFloat64;
            }
            else if( EQUAL(pszDataType, "ComplexLSB8") ||
                     EQUAL(pszDataType, "ComplexMSB8") )
            {
                eDT = GDT_CFloat32;
            }
            else if( EQUAL(pszDataType, "IEEE754LSBDouble") ||
                     EQUAL(pszDataType, "IEEE754MSBDouble")  )
            {
                eDT = GDT_Float64;
            }
            else if( EQUAL(pszDataType, "IEEE754LSBSingle") ||
                     EQUAL(pszDataType, "IEEE754MSBSingle") )
            {
                eDT = GDT_Float32;
            }
            // SignedBitString unhandled
            else if( EQUAL(pszDataType, "SignedByte") )
            {
                eDT = GDT_Byte;
                bSignedByte = true;
            }
            else if( EQUAL(pszDataType, "SignedLSB2") ||
                     EQUAL(pszDataType, "SignedMSB2") )
            {
                eDT = GDT_Int16;
            }
            else if( EQUAL(pszDataType, "SignedLSB4") ||
                     EQUAL(pszDataType, "SignedMSB4") )
            {
                eDT = GDT_Int32;
            }
            // SignedLSB8 and SignedMSB8 unhandled
            else if( EQUAL(pszDataType, "UnsignedByte") )
            {
                eDT = GDT_Byte;
            }
            else if( EQUAL(pszDataType, "UnsignedLSB2") ||
                     EQUAL(pszDataType, "UnsignedMSB2") )
            {
                eDT = GDT_UInt16;
            }
            else if( EQUAL(pszDataType, "UnsignedLSB4") ||
                     EQUAL(pszDataType, "UnsignedMSB4") )
            {
                eDT = GDT_UInt32;
            }
            // UnsignedLSB8 and UnsignedMSB8 unhandled
            else
            {
                CPLDebug("PDS4", "data_type = '%s' unhandled",
                         pszDataType);
                continue;
            }

            double dfValueOffset = CPLAtof(
                CPLGetXMLValue(psSubIter, "Element_Array.value_offset", "0"));
            double dfValueScale = CPLAtof(
                CPLGetXMLValue(psSubIter, "Element_Array.scaling_factor", "1"));

            // Parse Axis_Array elements
            char szOrder[4] = { 0 };
            int l_nBands = 1;
            int nLines = 0;
            int nSamples = 0;
            int nAxisFound = 0;
            int anElements[3] = { 0 };
            for( CPLXMLNode* psAxisIter = psSubIter->psChild;
                 psAxisIter != nullptr; psAxisIter = psAxisIter->psNext )
            {
                if( psAxisIter->eType != CXT_Element ||
                    strcmp(psAxisIter->pszValue, "Axis_Array") != 0 )
                {
                    continue;
                }
                const char* pszAxisName = CPLGetXMLValue(psAxisIter,
                                                         "axis_name", nullptr);
                const char* pszElements = CPLGetXMLValue(psAxisIter,
                                                         "elements", nullptr);
                const char* pszSequenceNumber = CPLGetXMLValue(psAxisIter,
                                                    "sequence_number", nullptr);
                if( pszAxisName == nullptr || pszElements == nullptr ||
                    pszSequenceNumber == nullptr )
                {
                    continue;
                }
                int nSeqNumber = atoi(pszSequenceNumber);
                if( nSeqNumber < 1 || nSeqNumber > nDIM )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid sequence_number = %s", pszSequenceNumber);
                    continue;
                }
                int nElements = atoi(pszElements);
                if( nElements <= 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid elements = %s", pszElements);
                    continue;
                }
                nSeqNumber --;
                if( szOrder[nSeqNumber] != '\0' )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid sequence_number = %s", pszSequenceNumber);
                    continue;
                }
                if( EQUAL(pszAxisName, "Band") && nDIM == 3 )
                {
                    szOrder[nSeqNumber] = 'B';
                    l_nBands = nElements;
                    anElements[nSeqNumber] = nElements;
                    nAxisFound ++;
                }
                else if( EQUAL(pszAxisName, "Line") )
                {
                    szOrder[nSeqNumber] = 'L';
                    nLines = nElements;
                    anElements[nSeqNumber] = nElements;
                    nAxisFound ++;
                }
                else if (EQUAL(pszAxisName, "Sample") )
                {
                    szOrder[nSeqNumber] = 'S';
                    nSamples = nElements;
                    anElements[nSeqNumber] = nElements;
                    nAxisFound ++;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Unsupported axis_name = %s", pszAxisName);
                    continue;
                }
            }
            if( nAxisFound != nDIM )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Found only %d Axis_Array elements. %d expected",
                         nAxisFound, nDIM);
                continue;
            }

            if( !GDALCheckDatasetDimensions(nSamples, nLines) ||
                !GDALCheckBandCount(l_nBands, FALSE) )
            {
                continue;
            }

            // Compute pixel, line and band spacing
            vsi_l_offset nSpacing = GDALGetDataTypeSizeBytes(eDT);
            int nPixelOffset = 0;
            int nLineOffset = 0;
            vsi_l_offset nBandOffset = 0;
            for( int i = nDIM - 1; i >= 0; i-- )
            {
                int nCountPreviousDim = i+1 < nDIM ? anElements[i+1] : 1;
                if( szOrder[i] == 'S' )
                {
                    if( nSpacing > static_cast<vsi_l_offset>(
                                            INT_MAX / nCountPreviousDim) )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Integer overflow");
                        delete poDS;
                        return nullptr;
                    }
                    nPixelOffset =
                        static_cast<int>(nSpacing * nCountPreviousDim);
                    nSpacing = nPixelOffset;
                }
                else if( szOrder[i] == 'L' )
                {
                    if( nSpacing > static_cast<vsi_l_offset>(
                                            INT_MAX / nCountPreviousDim) )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "Integer overflow");
                        delete poDS;
                        return nullptr;
                    }
                    nLineOffset =
                        static_cast<int>(nSpacing * nCountPreviousDim);
                    nSpacing = nLineOffset;
                }
                else
                {
                    nBandOffset = nSpacing * nCountPreviousDim;
                    nSpacing = nBandOffset;
                }
            }

            // Retrieve no data value
            bool bNoDataSet = false;
            double dfNoData = 0.0;
            std::vector<double> adfConstants;
            CPLXMLNode* psSC = CPLGetXMLNode(psSubIter, "Special_Constants");
            if( psSC )
            {
                const char* pszMC =
                    CPLGetXMLValue(psSC, "missing_constant", nullptr);
                if( pszMC )
                {
                    bNoDataSet = true;
                    dfNoData = CPLAtof(pszMC);
                }

                const char* apszConstantNames[] = {
                    "saturated_constant",
                    "missing_constant",
                    "error_constant",
                    "invalid_constant",
                    "unknown_constant",
                    "not_applicable_constant",
                    "high_instrument_saturation",
                    "high_representation_saturation",
                    "low_instrument_saturation",
                    "low_representation_saturation"
                };
                for(size_t i=0; i<CPL_ARRAYSIZE(apszConstantNames); i++ )
                {
                    const char* pszConstant =
                        CPLGetXMLValue(psSC, apszConstantNames[i], nullptr);
                    if( pszConstant )
                        adfConstants.push_back(CPLAtof(pszConstant));
                }
            }

            // Add subdatasets
            const int nSDSIdx = 1 + aosSubdatasets.size() / 2;
            aosSubdatasets.SetNameValue(
                CPLSPrintf("SUBDATASET_%d_NAME", nSDSIdx),
                CPLSPrintf("PDS4:%s:%d:%d",
                            osXMLFilename.c_str(),
                            nFAOIdx,
                            nArrayIdx));
            aosSubdatasets.SetNameValue(
                CPLSPrintf("SUBDATASET_%d_DESC", nSDSIdx),
                CPLSPrintf("Image file %s, array %s",
                            pszFilename,
                            pszArrayName ? pszArrayName :
                            pszArrayId ? pszArrayId :
                                CPLSPrintf("%d", nArrayIdx)));

            if( poDS->nBands != 0 )
                continue;

            const char* pszImageFullFilename =
                CPLFormFilename( CPLGetPath(osXMLFilename.c_str()),
                                 pszFilename, nullptr );
            VSILFILE* fp = VSIFOpenExL(pszImageFullFilename,
                (poOpenInfo->eAccess == GA_Update) ? "rb+" : "rb", true );
            if( fp == nullptr )
            {
                CPLError(CE_Warning, CPLE_FileIO,
                         "Cannt open %s: %s", pszImageFullFilename,
                         VSIGetLastErrorMsg());
                continue;
            }
            if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "PDS4:") )
                poDS->eAccess = poOpenInfo->eAccess;
            poDS->nRasterXSize = nSamples;
            poDS->nRasterYSize = nLines;
            poDS->m_osXMLFilename = osXMLFilename;
            poDS->m_osImageFilename = pszImageFullFilename;
            poDS->m_fpImage = fp;

            if( memcmp(szOrder, "BLS", 3) == 0 )
            {
                poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "BAND",
                                                  "IMAGE_STRUCTURE");
            }
            else if( memcmp(szOrder, "LSB", 3) == 0 )
            {
                poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                                  "IMAGE_STRUCTURE");
            }

            CPLXMLNode* psOS = CPLGetXMLNode(psSubIter, "Object_Statistics");
            const char* pszMin = nullptr;
            const char* pszMax = nullptr;
            const char* pszMean = nullptr;
            const char* pszStdDev = nullptr;
            if( psOS )
            {
                pszMin = CPLGetXMLValue(psOS, "minimum", nullptr);
                pszMax = CPLGetXMLValue(psOS, "maximum", nullptr);
                pszMean = CPLGetXMLValue(psOS, "mean", nullptr);
                pszStdDev = CPLGetXMLValue(psOS, "standard_deviation", nullptr);
            }

            for( int i = 0; i < l_nBands; i++ )
            {
                PDS4RawRasterBand *poBand = new PDS4RawRasterBand(
                    poDS,
                    i+1,
                    poDS->m_fpImage,
                    (bBottomToTop ) ?
                        nOffset + nBandOffset * i +
                            static_cast<vsi_l_offset>(nLines - 1) * nLineOffset :
                        nOffset + nBandOffset * i,
                    nPixelOffset,
                    (bBottomToTop ) ? -nLineOffset : nLineOffset,
                    eDT,
#ifdef CPL_LSB
                    bLSBOrder,
#else
                    !bLSBOrder,
#endif
                    true);
                if( bNoDataSet )
                {
                    poBand->SetNoDataValue(dfNoData);
                }
                if( bSignedByte )
                {
                    poBand->GDALRasterBand::SetMetadataItem(
                        "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
                }
                poBand->SetOffset( dfValueOffset );
                poBand->SetScale( dfValueScale );

                if( l_nBands == 1 )
                {
                    if( pszMin )
                    {
                        poBand->GDALRasterBand::SetMetadataItem(
                            "STATISTICS_MINIMUM", pszMin);
                    }
                    if( pszMax )
                    {
                        poBand->GDALRasterBand::SetMetadataItem(
                            "STATISTICS_MAXIMUM", pszMax);
                    }
                    if( pszMean )
                    {
                        poBand->GDALRasterBand::SetMetadataItem(
                            "STATISTICS_MEAN", pszMean);
                    }
                    if( pszStdDev )
                    {
                        poBand->GDALRasterBand::SetMetadataItem(
                            "STATISTICS_STDDEV", pszStdDev);
                    }
                }
                poDS->SetBand(i+1, poBand);

                // Only instantiate explicit mask band if we have at least one
                // special constant (that is not the missing_constant,
                // already exposed as nodata value)
                if( !GDALDataTypeIsComplex(eDT) &&
                    (CPLTestBool(CPLGetConfigOption("PDS4_FORCE_MASK", "NO")) ||
                     adfConstants.size() >= 2 ||
                     (adfConstants.size() == 1 && !bNoDataSet)) )
                {
                    poBand->SetMaskBand( new PDS4MaskBand(poBand, adfConstants) );
                }
            }
        }
    }

    if( nFAOIdxLookup < 0 && aosSubdatasets.size() > 2 )
    {
        poDS->GDALDataset::SetMetadata(aosSubdatasets.List(), "SUBDATASETS");
    }
    else if ( poDS->nBands == 0 )
    {
        delete poDS;
        return nullptr;
    }

    // Expose XML content in xml:PDS4 metadata domain
    GByte* pabyRet = nullptr;
    CPL_IGNORE_RET_VAL(
        VSIIngestFile(nullptr, osXMLFilename, &pabyRet, nullptr, 10*1024*1024) );
    if( pabyRet )
    {
        char* apszXML[2] = { reinterpret_cast<char*>(pabyRet), nullptr };
        poDS->GDALDataset::SetMetadata(apszXML, "xml:PDS4");
    }
    VSIFree(pabyRet);

/*--------------------------------------------------------------------------*/
/*  Parse georeferencing info                                               */
/*--------------------------------------------------------------------------*/
    poDS->ReadGeoreferencing(psProduct);

/*--------------------------------------------------------------------------*/
/*  Check for overviews                                                     */
/*--------------------------------------------------------------------------*/

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/*--------------------------------------------------------------------------*/
/*  Initialize any PAM information                                          */
/*--------------------------------------------------------------------------*/
    poDS->SetDescription( poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                         WriteGeoreferencing()                        */
/************************************************************************/

void PDS4Dataset::WriteGeoreferencing(CPLXMLNode* psCart)
{
    double adfX[4] = {0};
    double adfY[4] = {0};
    OGRSpatialReference oSRS;
    oSRS.SetFromUserInput(m_osWKT);
    CPLString osPrefix;
    const char* pszColon = strchr(psCart->pszValue, ':');
    if( pszColon )
        osPrefix.assign(psCart->pszValue, pszColon - psCart->pszValue + 1);

    // upper left
    adfX[0] = m_adfGeoTransform[0] + m_adfGeoTransform[1] / 2;
    adfY[0] = m_adfGeoTransform[3] - m_adfGeoTransform[5] / 2;

    // upper right
    adfX[1] = m_adfGeoTransform[0] + m_adfGeoTransform[1] * nRasterXSize - m_adfGeoTransform[1] / 2;
    adfY[1] = m_adfGeoTransform[3] - m_adfGeoTransform[5] / 2;

    // lower left
    adfX[2] = m_adfGeoTransform[0] + m_adfGeoTransform[1] / 2;
    adfY[2] = m_adfGeoTransform[3] + m_adfGeoTransform[5] * nRasterYSize + m_adfGeoTransform[5] / 2;

    // lower right
    adfX[3] = m_adfGeoTransform[0] + m_adfGeoTransform[1] * nRasterXSize - m_adfGeoTransform[1] / 2;
    adfY[3] = m_adfGeoTransform[3] + m_adfGeoTransform[5] * nRasterYSize + m_adfGeoTransform[5] / 2;

    if( !oSRS.IsGeographic() )
    {
        bool bHasBoundingBox = false;
        OGRSpatialReference* poSRSLongLat = oSRS.CloneGeogCS();
        OGRCoordinateTransformation* poCT =
            OGRCreateCoordinateTransformation(&oSRS, poSRSLongLat);
        if( poCT )
        {
            if( poCT->Transform(4, adfX, adfY) )
            {
                bHasBoundingBox = true;
            }
            delete poCT;
        }
        delete poSRSLongLat;
        if( !bHasBoundingBox )
        {
            // Write dummy values
            adfX[0] = -180.0;
            adfY[0] = 90.0;
            adfX[1] = 180.0;
            adfY[1] = 90.0;
            adfX[2] = -180.0;
            adfY[2] = -90.0;
            adfX[3] = 180.0;
            adfY[3] = -90.0;
        }
    }

    CPLXMLNode* psSD = CPLCreateXMLNode(psCart, CXT_Element,
            (osPrefix + "Spatial_Domain").c_str());
    CPLXMLNode* psBC = CPLCreateXMLNode(psSD, CXT_Element,
            (osPrefix + "Bounding_Coordinates").c_str());

    const char* pszBoundingDegrees = CSLFetchNameValue(
        m_papszCreationOptions, "BOUNDING_DEGREES");
    double dfWest = std::min(std::min(adfX[0], adfX[1]),
                             std::min(adfX[2], adfX[3]));
    double dfEast = std::max(std::max(adfX[0], adfX[1]), 
                             std::max(adfX[2], adfX[3]));
    double dfNorth = std::max(std::max(adfY[0], adfY[1]),
                              std::max(adfY[2], adfY[3]));
    double dfSouth = std::min(std::min(adfY[0], adfY[1]),
                              std::min(adfY[2], adfY[3]));
    if( pszBoundingDegrees )
    {
        char** papszTokens =
                CSLTokenizeString2(pszBoundingDegrees, ",", 0);
        if( CSLCount(papszTokens) == 4 )
        {
            dfWest  = CPLAtof(papszTokens[0]);
            dfSouth = CPLAtof(papszTokens[1]);
            dfEast  = CPLAtof(papszTokens[2]);
            dfNorth = CPLAtof(papszTokens[3]);
        }
        CSLDestroy(papszTokens);
    }

    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psBC,
            (osPrefix + "west_bounding_coordinate").c_str(),
            CPLSPrintf("%.18g", dfWest)), "unit", "deg");
    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psBC,
            (osPrefix + "east_bounding_coordinate").c_str(),
            CPLSPrintf("%.18g", dfEast )), "unit", "deg");
    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psBC,
            (osPrefix + "north_bounding_coordinate").c_str(),
            CPLSPrintf("%.18g", dfNorth)), "unit", "deg");
    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psBC,
            (osPrefix + "south_bounding_coordinate").c_str(),
            CPLSPrintf("%.18g", dfSouth)), "unit", "deg");

    CPLXMLNode* psSRI = CPLCreateXMLNode(psCart, CXT_Element,
                (osPrefix + "Spatial_Reference_Information").c_str());
    CPLXMLNode* psHCSD = CPLCreateXMLNode(psSRI, CXT_Element,
                (osPrefix + "Horizontal_Coordinate_System_Definition").c_str());
    CPLXMLNode* psPlanar = CPLCreateXMLNode(psHCSD, CXT_Element,
                (osPrefix + "Planar").c_str());
    CPLXMLNode* psMP = CPLCreateXMLNode(psPlanar, CXT_Element,
                (osPrefix + "Map_Projection").c_str());
    const char* pszProjection = oSRS.GetAttrValue("PROJECTION");
    CPLString pszPDS4ProjectionName = "";
    typedef std::pair<const char*, double> ProjParam;
    std::vector<ProjParam> aoProjParams;

    if( pszProjection == nullptr )
    {
        pszPDS4ProjectionName = "Equirectangular";
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian", 0.0));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin", 0.0));
    }

    else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
    {
        pszPDS4ProjectionName = "Equirectangular";
        aoProjParams.push_back(ProjParam("standard_parallel_1",
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 1.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        pszPDS4ProjectionName = "Lambert Conformal Conic";
        aoProjParams.push_back(ProjParam("scale_factor_at_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        pszPDS4ProjectionName = "Lambert Conformal Conic";
        aoProjParams.push_back(ProjParam("standard_parallel_1",
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0)));
        aoProjParams.push_back(ProjParam("standard_parallel_2",
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
    {
        pszPDS4ProjectionName = "Oblique Mercator";
        // Proj params defined later
    }

    else if( EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        pszPDS4ProjectionName = "Oblique Mercator";
        // Proj params defined later
    }

    else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        pszPDS4ProjectionName = "Polar Stereographic";
        aoProjParams.push_back(ProjParam("straight_vertical_longitude_from_pole",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("scale_factor_at_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_POLYCONIC) )
    {
        pszPDS4ProjectionName = "Polyconic";
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }
    else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
    {
        pszPDS4ProjectionName = "Sinusoidal";
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        pszPDS4ProjectionName = "Transverse Mercator";
        aoProjParams.push_back(ProjParam("scale_factor_at_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }
    else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
    {
        // Does not exist yet in schema
        pszPDS4ProjectionName = "Orthographic";
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
    {
        // Does not exist yet in schema
        pszPDS4ProjectionName = "Mercator";
        aoProjParams.push_back(ProjParam("scale_factor_at_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else if( EQUAL(pszProjection, SRS_PT_MERCATOR_2SP) )
    {
        // Does not exist yet in schema
        pszPDS4ProjectionName = "Mercator";
        aoProjParams.push_back(ProjParam("standard_parallel_1",
                oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0)));
        aoProjParams.push_back(ProjParam("longitude_of_central_meridian",
                oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0)));
        aoProjParams.push_back(ProjParam("latitude_of_projection_origin",
                oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0)));
    }

    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                         "Projection %s not supported",
                         pszProjection);
    }
    CPLCreateXMLElementAndValue(psMP,
                                (osPrefix + "map_projection_name").c_str(),
                                pszPDS4ProjectionName);
    CPLXMLNode* psProj = CPLCreateXMLNode(psMP, CXT_Element,
        CPLString(osPrefix + pszPDS4ProjectionName).replaceAll(' ','_'));
    for( size_t i = 0; i < aoProjParams.size(); i++ )
    {
        CPLXMLNode* psParam = CPLCreateXMLElementAndValue(psProj,
                (osPrefix + aoProjParams[i].first).c_str(),
                CPLSPrintf("%.18g", aoProjParams[i].second));
        if( !STARTS_WITH(aoProjParams[i].first, "scale_factor") )
        {
            CPLAddXMLAttributeAndValue(psParam, "unit", "deg");
        }
    }

    if( pszProjection &&
        EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
    {
        CPLCreateXMLElementAndValue(psProj,
            (osPrefix + "scale_factor_at_projection_origin").c_str(),
            CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 0.0)));
        CPLXMLNode* psOLA = CPLCreateXMLNode(psProj, CXT_Element,
                                (osPrefix + "Oblique_Line_Azimuth").c_str());
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLA,
                (osPrefix + "azimuthal_angle").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_AZIMUTH, 0.0))),
            "unit", "deg");;
        // Not completely sure of this
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLA,
                (osPrefix + "azimuth_measure_point_longitude").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0))),
            "unit", "deg");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psProj,
                (osPrefix + "latitude_of_projection_origin").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0))),
            "unit", "deg");
    }
    else if( pszProjection &&
        EQUAL(pszProjection, SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        CPLCreateXMLElementAndValue(psProj,
            (osPrefix + "scale_factor_at_projection_origin").c_str(),
            CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 0.0)));
        CPLXMLNode* psOLP = CPLCreateXMLNode(psProj, CXT_Element,
                                (osPrefix + "Oblique_Line_Point").c_str());
        CPLXMLNode* psOLPG1 = CPLCreateXMLNode(psOLP, CXT_Element,
                                (osPrefix + "Oblique_Line_Point_Group").c_str());
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLPG1,
                (osPrefix + "oblique_line_latitude").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0))),
            "unit", "deg");
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLPG1,
                (osPrefix + "oblique_line_longitude").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0))),
            "unit", "deg");
        CPLXMLNode* psOLPG2 = CPLCreateXMLNode(psOLP, CXT_Element,
                                (osPrefix + "Oblique_Line_Point_Group").c_str());
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLPG2,
                (osPrefix + "oblique_line_latitude").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_POINT_2, 0.0))),
            "unit", "deg");
        CPLAddXMLAttributeAndValue( 
            CPLCreateXMLElementAndValue(psOLPG2,
                (osPrefix + "oblique_line_longitude").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 0.0))),
            "unit", "deg");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psProj,
                (osPrefix + "latitude_of_projection_origin").c_str(),
                CPLSPrintf("%.18g", oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0))),
            "unit", "deg");
    }

    CPLXMLNode* psPCI = CPLCreateXMLNode(psPlanar, CXT_Element,
                    (osPrefix + "Planar_Coordinate_Information").c_str());
    CPLCreateXMLElementAndValue(psPCI,
        (osPrefix + "planar_coordinate_encoding_method").c_str(),
        "Coordinate Pair");
    CPLXMLNode* psPR = CPLCreateXMLNode(psPCI, CXT_Element,
                    (osPrefix + "Coordinate_Representation").c_str());
    const double dfLinearUnits = oSRS.GetLinearUnits();
    const double dfDegToMeter = oSRS.GetSemiMajor() * M_PI / 180.0;
    if( oSRS.IsGeographic() )
    {
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_resolution_x").c_str(),
                CPLSPrintf("%.18g", m_adfGeoTransform[1] * dfDegToMeter)),
            "unit", "m/pixel");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_resolution_y").c_str(),
                CPLSPrintf("%.18g", -m_adfGeoTransform[5] * dfDegToMeter)),
            "unit", "m/pixel");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_scale_x").c_str(),
                CPLSPrintf("%.18g", 1.0 / (m_adfGeoTransform[1]))),
            "unit", "pixel/deg");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_scale_y").c_str(),
                CPLSPrintf("%.18g", 1.0 / (-m_adfGeoTransform[5]))),
            "unit", "pixel/deg");
    }
    else if( oSRS.IsProjected() )
    {
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_resolution_x").c_str(),
                CPLSPrintf("%.18g", m_adfGeoTransform[1] * dfLinearUnits)),
            "unit", "m/pixel");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_resolution_y").c_str(),
                CPLSPrintf("%.18g", -m_adfGeoTransform[5] * dfLinearUnits)),
            "unit", "m/pixel");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_scale_x").c_str(),
                CPLSPrintf("%.18g", dfDegToMeter / (m_adfGeoTransform[1] * dfLinearUnits))),
            "unit", "pixel/deg");
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psPR,
                (osPrefix + "pixel_scale_y").c_str(),
                CPLSPrintf("%.18g", dfDegToMeter / (-m_adfGeoTransform[5] * dfLinearUnits))),
            "unit", "pixel/deg");
    }

    CPLXMLNode* psGT = CPLCreateXMLNode(psPlanar, CXT_Element,
                                        (osPrefix + "Geo_Transformation").c_str());
    const double dfFalseEasting =
        oSRS.GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0);
    const double dfFalseNorthing =
        oSRS.GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
    const double dfULX = -dfFalseEasting +
            m_adfGeoTransform[0] + 0.5 * m_adfGeoTransform[1];
    const double dfULY = -dfFalseNorthing + 
            m_adfGeoTransform[3] + 0.5 * m_adfGeoTransform[5];
    if( oSRS.IsGeographic() )
    {
        CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(psGT,
                    (osPrefix + "upperleft_corner_x").c_str(),
                    CPLSPrintf("%.18g", dfULX * dfDegToMeter)),
                "unit", "m");
        CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(psGT,
                    (osPrefix + "upperleft_corner_y").c_str(),
                    CPLSPrintf("%.18g", dfULY * dfDegToMeter)),
                "unit", "m");
    }
    else if( oSRS.IsProjected() )
    {
        CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(psGT,
                    (osPrefix + "upperleft_corner_x").c_str(),
                    CPLSPrintf("%.18g", dfULX * dfLinearUnits)),
                "unit", "m");
        CPLAddXMLAttributeAndValue(
                CPLCreateXMLElementAndValue(psGT,
                    (osPrefix + "upperleft_corner_y").c_str(),
                    CPLSPrintf("%.18g", dfULY * dfLinearUnits)),
                "unit", "m");
    }

    CPLXMLNode* psGM = CPLCreateXMLNode(psHCSD, CXT_Element,
                                        (osPrefix + "Geodetic_Model").c_str());
    const char* pszLatitudeType =
        CSLFetchNameValueDef(m_papszCreationOptions, "LATITUDE_TYPE",
                             "planetocentric");
    // Fix case
    if( EQUAL(pszLatitudeType, "planetocentric") )
        pszLatitudeType = "planetocentric";
    else if( EQUAL(pszLatitudeType, "planetographic") )
        pszLatitudeType = "planetographic";
    CPLCreateXMLElementAndValue(psGM,
                                (osPrefix + "latitude_type").c_str(),
                                pszLatitudeType);

    const char* pszDatum = oSRS.GetAttrValue("DATUM");
    if( pszDatum && STARTS_WITH(pszDatum, "D_") )
    {
        CPLCreateXMLElementAndValue(psGM,
                                    (osPrefix + "spheroid_name").c_str(),
                                    pszDatum + 2);
    }
    else if( pszDatum )
    {
        CPLCreateXMLElementAndValue(psGM,
                                    (osPrefix + "spheroid_name").c_str(),
                                    pszDatum);
    }

    double dfSemiMajor = oSRS.GetSemiMajor();
    double dfSemiMinor = oSRS.GetSemiMinor();
    const char* pszRadii = CSLFetchNameValue(m_papszCreationOptions, "RADII");
    if( pszRadii )
    {
        char** papszTokens = CSLTokenizeString2(pszRadii, " ,", 0);
        if( CSLCount(papszTokens) == 2 )
        {
            dfSemiMajor = CPLAtof(papszTokens[0]);
            dfSemiMinor = CPLAtof(papszTokens[1]);
        }
        CSLDestroy(papszTokens);
    }

    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psGM,
                (osPrefix + "semi_major_radius").c_str(),
                CPLSPrintf("%.18g", dfSemiMajor)),
        "unit", "m");
    // No, this is not a bug. The PDS4 semi_minor_radius is the minor radius
    // on the equatorial plane. Which in WKT doesn't really exist, so reuse
    // the WKT semi major
    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psGM,
                (osPrefix + "semi_minor_radius").c_str(),
                CPLSPrintf("%.18g", dfSemiMajor)),
        "unit", "m");
    CPLAddXMLAttributeAndValue(
        CPLCreateXMLElementAndValue(psGM,
                (osPrefix + "polar_radius").c_str(),
                CPLSPrintf("%.18g", dfSemiMinor)),
        "unit", "m");
    const char* pszLongitudeDirection =
        CSLFetchNameValueDef(m_papszCreationOptions, "LONGITUDE_DIRECTION",
                             "Positive East");
    // Fix case
    if( EQUAL(pszLongitudeDirection, "Positive East") )
        pszLongitudeDirection = "Positive East";
    else if( EQUAL(pszLongitudeDirection, "Positive West") )
        pszLongitudeDirection = "Positive West";
    CPLCreateXMLElementAndValue(psGM,
                                (osPrefix + "longitude_direction").c_str(),
                                pszLongitudeDirection);
}

/************************************************************************/
/*                         SubstituteVariables()                        */
/************************************************************************/

void PDS4Dataset::SubstituteVariables(CPLXMLNode* psNode, char** papszDict)
{
    if( psNode->eType == CXT_Text && psNode->pszValue &&
        strstr(psNode->pszValue, "${") )
    {
        CPLString osVal(psNode->pszValue);

        if( strstr(psNode->pszValue, "${TITLE}") != nullptr && 
            CSLFetchNameValue(papszDict, "VAR_TITLE") == nullptr )
        {
            const CPLString osTitle(CPLGetFilename(GetDescription()));
            CPLError(CE_Warning, CPLE_AppDefined,
                     "VAR_TITLE not defined. Using %s by default",
                     osTitle.c_str());
            osVal.replaceAll("${TITLE}", osTitle);
        }

        for( char** papszIter = papszDict; papszIter && *papszIter; papszIter++ )
        {
            if( STARTS_WITH_CI(*papszIter, "VAR_") )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
                if( pszKey && pszValue )
                {
                    const char* pszVarName = pszKey + strlen("VAR_");
                    osVal.replaceAll((CPLString("${") + pszVarName + "}").c_str(),
                                     pszValue);
                    osVal.replaceAll(CPLString(CPLString("${") + pszVarName + "}").tolower().c_str(),
                                     CPLString(pszValue).tolower());
                    CPLFree(pszKey);
                }
            }
        }
        if( osVal.find("${") != std::string::npos )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s could not be substituted", osVal.c_str());
        }
        CPLFree(psNode->pszValue);
        psNode->pszValue = CPLStrdup(osVal);
    }

    for(CPLXMLNode* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        SubstituteVariables(psIter, papszDict);
    }
}

/************************************************************************/
/*                         InitImageFile()                             */
/************************************************************************/

bool PDS4Dataset::InitImageFile()
{
    m_bMustInitImageFile = false;

    if( m_poExternalDS )
    {
        int nBlockXSize, nBlockYSize;
        GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
        const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
        const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
        const int nBlockSizeBytes = nBlockXSize * nBlockYSize * nDTSize;
        const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

        int bHasNoData = FALSE;
        double dfNoData = GetRasterBand(1)->GetNoDataValue(&bHasNoData);
        if( !bHasNoData )
            dfNoData = 0;

        if( nBands == 1 || EQUAL(m_osInterleave, "BSQ") )
        {
            // We need to make sure that blocks are written in the right order
            for( int i = 0; i < nBands; i++ )
            {
                if( m_poExternalDS->GetRasterBand(i+1)->Fill(dfNoData) !=
                                                                    CE_None )
                {
                    return false;
                }
            }
            m_poExternalDS->FlushCache();

            // Check that blocks are effectively written in expected order.
            GIntBig nLastOffset = 0;
            for( int i = 0; i < nBands; i++ )
            {
                for( int y = 0; y < l_nBlocksPerColumn; y++ )
                {
                    const char* pszBlockOffset =  m_poExternalDS->
                        GetRasterBand(i+1)->GetMetadataItem(
                            CPLSPrintf("BLOCK_OFFSET_%d_%d", 0, y), "TIFF");
                    if( pszBlockOffset )
                    {
                        GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                        if( i != 0 || y != 0 )
                        {
                            if( nOffset != nLastOffset + nBlockSizeBytes )
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Block %d,%d band %d not at expected "
                                         "offset",
                                         0, y, i+1);
                                return false;
                            }
                        }
                        nLastOffset = nOffset;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Block %d,%d band %d not at expected "
                                 "offset",
                                 0, y, i+1);
                        return false;
                    }
                }
            }
        }
        else
        {
            void* pBlockData = VSI_MALLOC_VERBOSE(nBlockSizeBytes);
            if( pBlockData == nullptr )
                return false;
            GDALCopyWords(&dfNoData, GDT_Float64, 0,
                          pBlockData, eDT, nDTSize,
                          nBlockXSize * nBlockYSize);
            for( int y = 0; y < l_nBlocksPerColumn; y++ )
            {
                for( int i = 0; i < nBands; i++ )
                {
                    if( m_poExternalDS->GetRasterBand(i+1)->
                                WriteBlock(0, y, pBlockData) != CE_None )
                    {
                        VSIFree(pBlockData);
                        return false;
                    }
                }
            }
            VSIFree(pBlockData);
            m_poExternalDS->FlushCache();

            // Check that blocks are effectively written in expected order.
            GIntBig nLastOffset = 0;
            for( int y = 0; y < l_nBlocksPerColumn; y++ )
            {
                const char* pszBlockOffset =  m_poExternalDS->
                    GetRasterBand(1)->GetMetadataItem(
                        CPLSPrintf("BLOCK_OFFSET_%d_%d", 0, y), "TIFF");
                if( pszBlockOffset )
                {
                    GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                    if( y != 0 )
                    {
                        if( nOffset != nLastOffset + nBlockSizeBytes * nBands )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                        "Block %d,%d not at expected "
                                        "offset",
                                        0, y);
                            return false;
                        }
                    }
                    nLastOffset = nOffset;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Block %d,%d not at expected "
                                "offset",
                                0, y);
                    return false;
                }
            }
        }

        return true;
    }

    int bHasNoData = FALSE;
    const double dfNoData = GetRasterBand(1)->GetNoDataValue(&bHasNoData);
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    const vsi_l_offset nFileSize =
            static_cast<vsi_l_offset>(nRasterXSize) * nRasterYSize * nBands *
                nDTSize;
    if( dfNoData == 0 || !bHasNoData )
    {
        if( VSIFTruncateL( m_fpImage, nFileSize ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Cannot create file of size " CPL_FRMT_GUIB " bytes",
                    nFileSize);
            return false;
        }
    }
    else
    {
        size_t nLineSize = static_cast<size_t>(nRasterXSize) * nDTSize;
        void* pData = VSI_MALLOC_VERBOSE(nLineSize);
        if( pData == nullptr )
            return false;
        GDALCopyWords(&dfNoData, GDT_Float64, 0,
                      pData, eDT, nDTSize,
                      nRasterXSize);
#ifdef CPL_MSB
        if( GDALDataTypeIsComplex(eDT) )
        {
            GDALSwapWords(pData, nDTSize / 2, nRasterXSize * 2, nDTSize / 2);
        }
        else
        {
            GDALSwapWords(pData, nDTSize, nRasterXSize, nDTSize);
        }
#endif
        for( vsi_l_offset i = 0; i <
                static_cast<vsi_l_offset>(nRasterYSize) * nBands; i++ )
        {
            size_t nBytesWritten = VSIFWriteL(pData, 1, nLineSize, m_fpImage);
            if( nBytesWritten != nLineSize )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                        "Cannot create file of size " CPL_FRMT_GUIB " bytes",
                        nFileSize);
                VSIFree(pData);
                return false;
            }
        }
        VSIFree(pData);
    }
    return true;
}

/************************************************************************/
/*                          GetSpecialConstants()                       */
/************************************************************************/

static CPLXMLNode* GetSpecialConstants(const CPLString& osPrefix,
                                       CPLXMLNode* psFileAreaObservational)
{
    for(CPLXMLNode* psIter = psFileAreaObservational->psChild;
                psIter; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            STARTS_WITH(psIter->pszValue, (osPrefix + "Array").c_str()) )
        {
            CPLXMLNode* psSC =
                CPLGetXMLNode(psIter, (osPrefix + "Special_Constants").c_str());
            if( psSC )
            {
                CPLXMLNode* psNext = psSC->psNext;
                psSC->psNext = nullptr;
                CPLXMLNode* psRet = CPLCloneXMLTree(psSC);
                psSC->psNext = psNext;
                return psRet;
            }
        }
    }
    return nullptr;
}

/************************************************************************/
/*                             WriteHeader()                            */
/************************************************************************/

void PDS4Dataset::WriteHeader()
{
    CPLXMLNode* psRoot;
    CPLString osTemplateFilename = CSLFetchNameValueDef(m_papszCreationOptions,
                                                      "TEMPLATE", "");
    if( !osTemplateFilename.empty() )
    {
        if( STARTS_WITH(osTemplateFilename, "http://") ||
            STARTS_WITH(osTemplateFilename, "https://") )
        {
            osTemplateFilename = "/vsicurl_streaming/" + osTemplateFilename;
        }
        psRoot = CPLParseXMLFile(osTemplateFilename);
    }
    else if( !m_osXMLPDS4.empty() )
        psRoot = CPLParseXMLString(m_osXMLPDS4);
    else
    {
        const char* pszDefaultTemplateFilename =
                                CPLFindFile("gdal", "pds4_template.xml");
        if( pszDefaultTemplateFilename == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find pds4_template.xml and TEMPLATE "
                     "creation option not specified");
            return;
        }
        psRoot = CPLParseXMLFile(pszDefaultTemplateFilename);
    }
    if( psRoot == nullptr )
        return;
    CPLXMLTreeCloser oCloser(psRoot);
    CPLString osPrefix;
    CPLXMLNode* psProduct = CPLGetXMLNode(psRoot, "=Product_Observational");
    if( psProduct == nullptr )
    {
        psProduct = CPLGetXMLNode(psRoot, "=pds:Product_Observational");
        if( psProduct )
            osPrefix = "pds:";
    }
    if( psProduct == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Product_Observational element in template");
        return;
    }

    if( !m_osWKT.empty() &&
            CSLFetchNameValue(m_papszCreationOptions, "VAR_TARGET") == nullptr )
    {
        OGRSpatialReference oSRS;
        oSRS.SetFromUserInput(m_osWKT);
        const char* pszTarget = nullptr;
        if( fabs(oSRS.GetSemiMajor() - 6378137) < 0.001 * 6378137 )
        {
            pszTarget = "Earth";
            m_papszCreationOptions = CSLSetNameValue(
                m_papszCreationOptions, "VAR_TARGET_TYPE", "Planet");
        }
        else
        {
            const char* pszDatum = oSRS.GetAttrValue("DATUM");
            if( pszDatum && STARTS_WITH(pszDatum, "D_") )
            {
                pszTarget = pszDatum + 2;
            }
            else if( pszDatum )
            {
                pszTarget = pszDatum;
            }
        }
        if( pszTarget )
        {
            m_papszCreationOptions = CSLSetNameValue(
                m_papszCreationOptions, "VAR_TARGET", pszTarget);
        }
    }
    SubstituteVariables(psProduct, m_papszCreationOptions);

    CPLXMLNode* psDisciplineArea = CPLGetXMLNode(psProduct,
        (osPrefix + "Observation_Area." + osPrefix + "Discipline_Area").c_str());
    if( !(m_bGotTransform && !m_osWKT.empty()) )
    {
        // if we have no georeferencing, strip any existing georeferencing
        // from the template
        if( psDisciplineArea )
        {
            CPLXMLNode* psPrev = nullptr;
            for( CPLXMLNode* psIter = psDisciplineArea->psChild;
                    psIter != nullptr; psPrev = psIter, psIter = psIter->psNext )
            {
                if( psIter->eType == CXT_Element &&
                    (strcmp(psIter->pszValue, "Cartography") == 0 ||
                     strcmp(psIter->pszValue, "cart:Cartography") == 0) )
                {
                    if( psPrev )
                    {
                        psPrev->psNext = psIter->psNext;
                    }
                    else
                    {
                        psDisciplineArea->psChild = psIter->psNext;
                    }
                    psIter->psNext = nullptr;
                    CPLDestroyXMLNode(psIter);
                    break;
                }
            }
        }
    }
    else
    {
        if( psDisciplineArea == nullptr )
        {
            CPLXMLNode* psTI = CPLGetXMLNode(psProduct,
                (osPrefix + "Observation_Area." +
                 osPrefix + "Target_Identification").c_str());
            if( psTI == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Target_Identification element in template");
                return;
            }
            psDisciplineArea = CPLCreateXMLNode(nullptr, CXT_Element,
                                        (osPrefix + "Discipline_Area").c_str());
            if( psTI->psNext )
                psDisciplineArea->psNext = psTI->psNext;
            psTI->psNext = psDisciplineArea;
        }
        CPLXMLNode* psCart = CPLGetXMLNode(psDisciplineArea, "cart:Cartography");
        if( psCart == nullptr )
            psCart = CPLGetXMLNode(psDisciplineArea, "Cartography");
        if( psCart == nullptr )
        {
            psCart = CPLCreateXMLNode(psDisciplineArea,
                                      CXT_Element, "cart:Cartography");
            if( CPLGetXMLNode(psProduct, "xmlns:cart") == nullptr )
            {
                CPLXMLNode* psNS = CPLCreateXMLNode( nullptr, CXT_Attribute,
                                                     "xmlns:cart" );
                CPLCreateXMLNode(psNS, CXT_Text,
                                 "http://pds.nasa.gov/pds4/cart/v1");
                CPLAddXMLChild(psProduct, psNS);
                CPLXMLNode* psSchemaLoc =
                    CPLGetXMLNode(psProduct, "xsi:schemaLocation");
                if( psSchemaLoc != nullptr && psSchemaLoc->psChild != nullptr &&
                    psSchemaLoc->psChild->pszValue != nullptr )
                {
                    CPLString osNewVal(psSchemaLoc->psChild->pszValue);
                    osNewVal += " http://pds.nasa.gov/pds4/cart/v1 "
                        "https://pds.nasa.gov/pds4/cart/v1/PDS4_CART_1700.xsd";
                    CPLFree(psSchemaLoc->psChild->pszValue);
                    psSchemaLoc->psChild->pszValue = CPLStrdup(osNewVal);
                }
            }
        }
        else
        {
            if( psCart->psChild )
            {
                CPLDestroyXMLNode(psCart->psChild);
                psCart->psChild = nullptr;
            }
        }
        WriteGeoreferencing(psCart);
    }

    if( m_bStripFileAreaObservationalFromTemplate )
    {
        m_bStripFileAreaObservationalFromTemplate = false;
        CPLXMLNode* psObservationArea = nullptr;
        CPLXMLNode* psPrev = nullptr;
        CPLXMLNode* psTemplateSpecialConstants = nullptr;
        for( CPLXMLNode* psIter = psProduct->psChild; psIter != nullptr; )
        {
            if( psIter->eType == CXT_Element &&
                psIter->pszValue == osPrefix + "Observation_Area" )
            {
                psObservationArea = psIter;
                psPrev = psIter;
                psIter = psIter->psNext;
            }
            else if( psIter->eType == CXT_Element &&
                (psIter->pszValue == osPrefix + "File_Area_Observational" ||
                 psIter->pszValue == osPrefix + "File_Area_Observational_Supplemental") )
            {
                if( psIter->pszValue == osPrefix + "File_Area_Observational" )
                {
                    psTemplateSpecialConstants = GetSpecialConstants(osPrefix,
                                                                     psIter);
                }
                if( psPrev )
                    psPrev->psNext = psIter->psNext;
                else
                {
                    CPLAssert( psProduct->psChild == psIter );
                    psProduct->psChild = psIter->psNext;
                }
                CPLXMLNode* psNext = psIter->psNext;
                psIter->psNext = nullptr;
                CPLDestroyXMLNode(psIter);
                psIter = psNext;
            }
            else
            {
                psPrev = psIter;
                psIter = psIter->psNext;
            }
        }
        if( psObservationArea == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find Observation_Area in template");
            CPLDestroyXMLNode( psTemplateSpecialConstants );
            return;
        }
        CPLXMLNode* psFAOPrev = psObservationArea;
        while( psFAOPrev->psNext != nullptr &&
               psFAOPrev->psNext->eType == CXT_Comment )
        {
            psFAOPrev = psFAOPrev->psNext;
        }
        if( psFAOPrev->psNext != nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected content found after Observation_Area in template");
            CPLDestroyXMLNode( psTemplateSpecialConstants );
            return;
        }

        CPLXMLNode* psFAO = CPLCreateXMLNode(nullptr, CXT_Element,
                            (osPrefix + "File_Area_Observational").c_str());
        psFAOPrev->psNext = psFAO;
        CPLXMLNode* psFile = CPLCreateXMLNode(psFAO, CXT_Element,
                                              (osPrefix + "File").c_str());
        CPLCreateXMLElementAndValue(psFile, (osPrefix + "file_name").c_str(),
                                    CPLGetFilename(m_osImageFilename));
        const char* pszArrayType = CSLFetchNameValueDef(m_papszCreationOptions,
                                            "ARRAY_TYPE", "Array_3D_Image");
        const bool bIsArray2D = STARTS_WITH(pszArrayType, "Array_2D");
        CPLXMLNode* psArray = CPLCreateXMLNode(psFAO, CXT_Element,
                                            (osPrefix + pszArrayType).c_str());

        const char* pszLocalIdentifier = CPLGetXMLValue(
            psDisciplineArea,
            "disp:Display_Settings.Local_Internal_Reference."
                                            "local_identifier_reference",
            "image");
        CPLCreateXMLElementAndValue(psArray,
                                    (osPrefix + "local_identifier").c_str(),
                                    pszLocalIdentifier);

        int nOffset = 0;
        if( m_poExternalDS )
        {
            const char* pszOffset = m_poExternalDS->GetRasterBand(1)->
                                GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF");
            if( pszOffset )
                nOffset = atoi(pszOffset);
        }
        CPLAddXMLAttributeAndValue(
            CPLCreateXMLElementAndValue(psArray,
                                        (osPrefix + "offset").c_str(),
                                        CPLSPrintf("%d", nOffset)),
            "unit", "byte");
        CPLCreateXMLElementAndValue(psArray, (osPrefix + "axes").c_str(),
                                                    (bIsArray2D) ? "2" : "3");
        CPLCreateXMLElementAndValue(psArray,
                                    (osPrefix + "axis_index_order").c_str(),
                                    "Last Index Fastest");
        CPLXMLNode* psElementArray = CPLCreateXMLNode(psArray, CXT_Element,
                                        (osPrefix + "Element_Array").c_str());
        GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
        const char* pszDataType =
            (eDT == GDT_Byte) ? "UnsignedByte" :
            (eDT == GDT_UInt16) ? "UnsignedLSB2" :
            (eDT == GDT_Int16) ? "SignedLSB2" :
            (eDT == GDT_UInt32) ? "UnsignedLSB4" :
            (eDT == GDT_Int32) ? "SignedLSB4" :
            (eDT == GDT_Float32) ? "IEEE754LSBSingle" :
            (eDT == GDT_Float64) ? "IEEE754LSBDouble" :
            (eDT == GDT_CFloat32) ? "ComplexLSB8" :
            (eDT == GDT_CFloat64) ? "ComplexLSB16" :
                                    "should not happen";
        CPLCreateXMLElementAndValue(psElementArray,
                                    (osPrefix + "data_type").c_str(),
                                    pszDataType);

        int bHasScale = FALSE;
        double dfScale = GetRasterBand(1)->GetScale(&bHasScale);
        if( bHasScale && dfScale != 1.0 )
        {
            CPLCreateXMLElementAndValue(psElementArray,
                                (osPrefix + "scaling_factor").c_str(),
                                CPLSPrintf("%.18g", dfScale));
        }

        int bHasOffset = FALSE;
        double dfOffset = GetRasterBand(1)->GetOffset(&bHasOffset);
        if( bHasOffset && dfOffset != 1.0 )
        {
            CPLCreateXMLElementAndValue(psElementArray,
                                (osPrefix + "value_offset").c_str(),
                                CPLSPrintf("%.18g", dfOffset));
        }

        // Axis definitions
        {
            CPLXMLNode* psAxis = CPLCreateXMLNode(psArray, CXT_Element,
                                        (osPrefix + "Axis_Array").c_str());
            CPLCreateXMLElementAndValue(psAxis,
                                    (osPrefix + "axis_name").c_str(),
                                    EQUAL(m_osInterleave, "BSQ") ? "Band" :
                                    /* EQUAL(m_osInterleave, "BIL") ? "Line" : */
                                                                   "Line");
            CPLCreateXMLElementAndValue(psAxis,
                            (osPrefix + "elements").c_str(),
                            CPLSPrintf("%d",
                                EQUAL(m_osInterleave, "BSQ") ? nBands :
                                /* EQUAL(m_osInterleave, "BIL") ? nRasterYSize : */
                                                               nRasterYSize
                            ));
            CPLCreateXMLElementAndValue(psAxis,
                                (osPrefix + "sequence_number").c_str(), "1");
        }
        {
            CPLXMLNode* psAxis = CPLCreateXMLNode(psArray, CXT_Element,
                                        (osPrefix + "Axis_Array").c_str());
            CPLCreateXMLElementAndValue(psAxis,
                                    (osPrefix + "axis_name").c_str(),
                                    EQUAL(m_osInterleave, "BSQ") ? "Line" :
                                    EQUAL(m_osInterleave, "BIL") ? "Band" :
                                                                   "Sample");
            CPLCreateXMLElementAndValue(psAxis,
                            (osPrefix + "elements").c_str(),
                            CPLSPrintf("%d",
                                EQUAL(m_osInterleave, "BSQ") ? nRasterYSize :
                                EQUAL(m_osInterleave, "BIL") ? nBands :
                                                               nRasterXSize
                            ));
            CPLCreateXMLElementAndValue(psAxis,
                                (osPrefix + "sequence_number").c_str(), "2");
        }
        if( !bIsArray2D )
        {
            CPLXMLNode* psAxis = CPLCreateXMLNode(psArray, CXT_Element,
                                        (osPrefix + "Axis_Array").c_str());
            CPLCreateXMLElementAndValue(psAxis,
                                    (osPrefix + "axis_name").c_str(),
                                    EQUAL(m_osInterleave, "BSQ") ? "Sample" :
                                    EQUAL(m_osInterleave, "BIL") ? "Sample" :
                                                                   "Band");
            CPLCreateXMLElementAndValue(psAxis,
                            (osPrefix + "elements").c_str(),
                            CPLSPrintf("%d",
                                EQUAL(m_osInterleave, "BSQ") ? nRasterXSize :
                                EQUAL(m_osInterleave, "BIL") ? nRasterXSize :
                                                               nBands
                            ));
            CPLCreateXMLElementAndValue(psAxis,
                                (osPrefix + "sequence_number").c_str(), "3");
        }

        int bHasNoData = FALSE;
        double dfNoData = GetRasterBand(1)->GetNoDataValue(&bHasNoData);
        if( psTemplateSpecialConstants )
        {
            CPLAddXMLChild(psArray, psTemplateSpecialConstants);
            if( bHasNoData )
            {
                CPLXMLNode* psMC = CPLGetXMLNode(
                    psTemplateSpecialConstants,
                    (osPrefix + "missing_constant").c_str());
                if( psMC != nullptr )
                {
                    if( psMC->psChild && psMC->psChild->eType == CXT_Text )
                    {
                        CPLFree( psMC->psChild->pszValue );
                        psMC->psChild->pszValue =
                                    CPLStrdup(CPLSPrintf("%.18g", dfNoData));
                    }
                }
                else
                {
                    CPLXMLNode* psSaturatedConstant = CPLGetXMLNode(
                        psTemplateSpecialConstants,
                        (osPrefix + "saturated_constant").c_str());
                    psMC = CPLCreateXMLElementAndValue(nullptr,
                        (osPrefix + "missing_constant").c_str(),
                        CPLSPrintf("%.18g", dfNoData));
                    if( psSaturatedConstant )
                    {
                        CPLXMLNode* psNext = psSaturatedConstant->psNext;
                        psSaturatedConstant->psNext = psMC;
                        psMC->psNext = psNext;
                    }
                    else
                    {
                        CPLXMLNode* psNext = psTemplateSpecialConstants->psChild;
                        psTemplateSpecialConstants->psChild = psMC;
                        psMC->psNext = psNext;
                    }
                }
            }
        }
        else if( bHasNoData )
        {
            CPLXMLNode* psSC = CPLCreateXMLNode(psArray, CXT_Element,
                                    (osPrefix + "Special_Constants").c_str());
            CPLCreateXMLElementAndValue(psSC,
                                (osPrefix + "missing_constant").c_str(),
                                CPLSPrintf("%.18g", dfNoData));
        }
    }

    CPLSerializeXMLTreeToFile(psRoot, GetDescription());
}

/************************************************************************/
/*                         GDALRegister_PDS4()                          */
/************************************************************************/

GDALDataset *PDS4Dataset::Create(const char *pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszOptions)
{
    if( !(eType == GDT_Byte || eType == GDT_Int16 || eType == GDT_UInt16 ||
          eType == GDT_Int32 || eType == GDT_UInt32 || eType == GDT_Float32 ||
          eType == GDT_Float64 || eType == GDT_CFloat32 ||
          eType == GDT_CFloat64) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The ISIS2 driver does not supporting creating files of type %s.",
                 GDALGetDataTypeName( eType ) );
        return nullptr;
    }

    if( nBands == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid number of bands");
        return nullptr;
    }

    const char* pszArrayType = CSLFetchNameValueDef(papszOptions,
                                    "ARRAY_TYPE", "Array_3D_Image");
    const bool bIsArray2D = STARTS_WITH(pszArrayType, "Array_2D");
    if( nBands > 1 && bIsArray2D )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ARRAY_TYPE=%s is not supported for a multi-band raster",
                 pszArrayType);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Compute pixel, line and band offsets                            */
/* -------------------------------------------------------------------- */
    const int nItemSize = GDALGetDataTypeSizeBytes(eType);
    int nLineOffset, nPixelOffset;
    vsi_l_offset nBandOffset;

    const char* pszInterleave = CSLFetchNameValueDef(
        papszOptions, "INTERLEAVE", "BSQ");
    if( bIsArray2D )
        pszInterleave = "BIP";

    if( EQUAL(pszInterleave,"BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        if( nPixelOffset > INT_MAX / nBands )
        {
            return nullptr;
        }
        nLineOffset = nPixelOffset * nXSize;
        nBandOffset = nItemSize;
    }
    else if( EQUAL(pszInterleave,"BSQ") )
    {
        nPixelOffset = nItemSize;
        if( nPixelOffset > INT_MAX / nXSize )
        {
            return nullptr;
        }
        nLineOffset = nPixelOffset * nXSize;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nYSize;
    }
    else if( EQUAL(pszInterleave, "BIL") )
    {
        nPixelOffset = nItemSize;
        if( nPixelOffset > INT_MAX / nBands ||
            nPixelOffset * nBands > INT_MAX / nXSize )
        {
            return nullptr;
        }
        nLineOffset = nItemSize * nBands * nXSize;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize) * nXSize;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid value for INTERLEAVE");
        return nullptr;
    }

    const char* pszImageFormat = CSLFetchNameValueDef(papszOptions,
                                                       "IMAGE_FORMAT",
                                                       "RAW");
    const char* pszImageExtension = CSLFetchNameValueDef(papszOptions,
        "IMAGE_EXTENSION", EQUAL(pszImageFormat, "RAW") ? "img" : "tif");
    CPLString osImageFilename(CSLFetchNameValueDef(papszOptions,
        "IMAGE_FILENAME", CPLResetExtension(pszFilename, pszImageExtension)));

    GDALDataset* poExternalDS = nullptr;
    VSILFILE* fpImage = nullptr;
    if( EQUAL(pszImageFormat, "GEOTIFF") )
    {
        if( EQUAL(pszInterleave, "BIL") )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "INTERLEAVE=BIL not supported for GeoTIFF in PDS4" );
            return nullptr;
        }
        GDALDriver* poDrv = static_cast<GDALDriver*>(
                                            GDALGetDriverByName("GTiff"));
        if( poDrv == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot find GTiff driver" );
            return nullptr;
        }
        char** papszGTiffOptions = nullptr;
#ifdef notdef
        // In practice I can't see which option we can really use
        const char* pszGTiffOptions = CSLFetchNameValueDef(papszOptions,
                                                    "GEOTIFF_OPTIONS", "");
        char** papszTokens = CSLTokenizeString2( pszGTiffOptions, ",", 0 );
        if( CPLFetchBool(papszTokens, "TILED", false) )
        {
            CSLDestroy(papszTokens);
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Tiled GeoTIFF is not supported for PDS4" );
            return NULL;
        }
        if( !EQUAL(CSLFetchNameValueDef(papszTokens, "COMPRESS", "NONE"), "NONE") )
        {
            CSLDestroy(papszTokens);
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Compressed GeoTIFF is not supported for PDS4" );
            return NULL;
        }
        papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                            "ENDIANNESS", "LITTLE");
        for( int i = 0; papszTokens[i] != NULL; i++ )
        {
            papszGTiffOptions = CSLAddString(papszGTiffOptions,
                                             papszTokens[i]);
        }
        CSLDestroy(papszTokens);
#endif

        papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                "INTERLEAVE", EQUAL(pszInterleave, "BSQ") ? "BAND" : "PIXEL");
        // Will make sure that our blocks at nodata are not optimized
        // away but indeed well written
        papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "YES");
        if( nBands > 1 && EQUAL(pszInterleave, "BSQ") )
        {
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "BLOCKYSIZE", "1");
        }

        poExternalDS = poDrv->Create( osImageFilename, nXSize, nYSize,
                                      nBands,
                                      eType, papszGTiffOptions );
        CSLDestroy(papszGTiffOptions);
        if( poExternalDS == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot create %s",
                      osImageFilename.c_str() );
            return nullptr;
        }
    }
    else
    {
        fpImage = VSIFOpenL(osImageFilename, "wb");
        if( fpImage == nullptr )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s",
                    osImageFilename.c_str());
            return nullptr;
        }
    }

    PDS4Dataset* poDS = new PDS4Dataset();
    poDS->SetDescription(pszFilename);
    poDS->m_bMustInitImageFile = true;
    poDS->m_fpImage = fpImage;
    poDS->m_poExternalDS = poExternalDS;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->m_osImageFilename = osImageFilename;
    poDS->m_bWriteHeader = true;
    poDS->m_bStripFileAreaObservationalFromTemplate = true;
    poDS->m_osInterleave = pszInterleave;
    poDS->m_papszCreationOptions = CSLDuplicate(papszOptions);
    poDS->m_bUseSrcLabel = CPLFetchBool(papszOptions, "USE_SRC_LABEL", true);

    if( EQUAL(pszInterleave, "BIP") )
    {
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                           "IMAGE_STRUCTURE");
    }
    else if( EQUAL(pszInterleave, "BSQ") )
    {
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "BAND",
                                           "IMAGE_STRUCTURE");
    }

    for( int i = 0; i < nBands; i++ )
    {
        if( poDS->m_poExternalDS != nullptr )
        {
            PDS4WrapperRasterBand* poBand =
                new PDS4WrapperRasterBand(
                            poDS->m_poExternalDS->GetRasterBand( i+1 ) );
            poDS->SetBand(i+1, poBand);
        }
        else
        {
            PDS4RawRasterBand *poBand = new
                    PDS4RawRasterBand(poDS, i+1, poDS->m_fpImage,
                                        nBandOffset * i,
                                        nPixelOffset,
                                        nLineOffset,
                                        eType,
#ifdef CPL_LSB
                                        TRUE,
#else
                                        FALSE, // force LSB order
#endif
                                        true);
            poDS->SetBand(i+1, poBand);
        }
    }

    return poDS;
}

/************************************************************************/
/*                      PDS4GetUnderlyingDataset()                      */
/************************************************************************/

static GDALDataset* PDS4GetUnderlyingDataset( GDALDataset* poSrcDS )
{
    if( poSrcDS->GetDriver() != nullptr &&
        poSrcDS->GetDriver() == GDALGetDriverByName("VRT") )
    {
        VRTDataset* poVRTDS = reinterpret_cast<VRTDataset* >(poSrcDS);
        poSrcDS = poVRTDS->GetSingleSimpleSource();
    }

    return poSrcDS;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset* PDS4Dataset::CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int /*bStrict*/,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
{
    const char* pszImageFormat = CSLFetchNameValueDef(papszOptions,
                                                       "IMAGE_FORMAT",
                                                       "RAW");
    GDALDataset* poSrcUnderlyingDS = PDS4GetUnderlyingDataset(poSrcDS);
    if( poSrcUnderlyingDS == nullptr )
        poSrcUnderlyingDS = poSrcDS;
    if( EQUAL(pszImageFormat, "GEOTIFF") &&
        strcmp(poSrcUnderlyingDS->GetDescription(),
               CSLFetchNameValueDef(papszOptions, "IMAGE_FILENAME",
                                CPLResetExtension(pszFilename, "tif"))) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Output file has same name as input file");
        return nullptr;
    }
    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    PDS4Dataset *poDS = reinterpret_cast<PDS4Dataset*>(
        Create( pszFilename, nXSize, nYSize, nBands, eType, papszOptions ));
    if( poDS == nullptr )
        return nullptr;

    double adfGeoTransform[6] = { 0.0 };
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        poDS->SetGeoTransform( adfGeoTransform );
    }

    if( poSrcDS->GetProjectionRef() != nullptr
        && strlen(poSrcDS->GetProjectionRef()) > 0 )
    {
        poDS->SetProjection( poSrcDS->GetProjectionRef() );
    }

    for(int i=1;i<=nBands;i++)
    {
        int bHasNoData = false;
        const double dfNoData = poSrcDS->GetRasterBand(i)->GetNoDataValue(&bHasNoData);
        if( bHasNoData )
            poDS->GetRasterBand(i)->SetNoDataValue(dfNoData);

        const double dfOffset = poSrcDS->GetRasterBand(i)->GetOffset();
        if( dfOffset != 0.0 )
            poDS->GetRasterBand(i)->SetOffset(dfOffset);

        const double dfScale = poSrcDS->GetRasterBand(i)->GetScale();
        if( dfScale != 1.0 )
            poDS->GetRasterBand(i)->SetScale(dfScale);
    }

    if( poDS->m_bUseSrcLabel )
    {
        char** papszMD_PDS4 = poSrcDS->GetMetadata("xml:PDS4");
        if( papszMD_PDS4 != nullptr )
        {
            poDS->SetMetadata( papszMD_PDS4, "xml:PDS4" );
        }
    }

    if( poDS->m_poExternalDS == nullptr )
    {
        // We don't need to initialize the imagery as we are going to copy it
        // completely
        poDS->m_bMustInitImageFile = false;
    }
    CPLErr eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDS,
                                           nullptr, pfnProgress, pProgressData );
    poDS->FlushCache();
    if( eErr != CE_None )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}


/************************************************************************/
/*                         GDALRegister_PDS4()                          */
/************************************************************************/

void GDALRegister_PDS4()

{
    if( GDALGetDriverByName( "PDS4" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PDS4" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "NASA Planetary Data System 4" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_pds4.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xml" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CFloat32 CFloat64" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='IMAGE_FILENAME' type='string' description="
                    "'Image filename'/>"
"  <Option name='IMAGE_EXTENSION' type='string' description="
                    "'Extension of the binary raw/geotiff file'/>"
"  <Option name='IMAGE_FORMAT' type='string-select' "
                    "description='Format of the image file' default='RAW'>"
"     <Value>RAW</Value>"
"     <Value>GEOTIFF</Value>"
"  </Option>"
#ifdef notdef
"  <Option name='GEOTIFF_OPTIONS' type='string' "
    "description='Comma separated list of KEY=VALUE tuples to forward "
    "to the GeoTIFF driver'/>"
#endif
"  <Option name='INTERLEAVE' type='string-select' description="
                    "'Pixel organization' default='BSQ'>"
"     <Value>BSQ</Value>"
"     <Value>BIP</Value>"
"     <Value>BIL</Value>"
"  </Option>"
"  <Option name='VAR_*' type='string' description="
                    "'Value to substitute to a variable in the template'/>"
"  <Option name='TEMPLATE' type='string' description="
                    "'.xml template to use'/>"
"  <Option name='USE_SRC_LABEL' type='boolean'"
    "description='Whether to use source label in PDS4 to PDS4 conversions' "
    "default='YES'/>"
"  <Option name='LATITUDE_TYPE' type='string-select' "
    "description='Value of latitude_type' default='planetocentric'>"
"     <Value>planetocentric</Value>"
"     <Value>planetographic</Value>"
"  </Option>"
"  <Option name='LONGITUDE_DIRECTION' type='string-select' "
    "description='Value of longitude_direction' "
    "default='Positive East'>"
"     <Value>Positive East</Value>"
"     <Value>Positive West</Value>"
"  </Option>"
"  <Option name='RADII' type='string' description='Value of form "
    "semi_major_radius,semi_minor_radius to override the ones of the SRS'/>"
"  <Option name='ARRAY_TYPE' type='string-select' description='Name of the "
            "Array XML element' default='Array_3D_Image'>"
"     <Value>Array</Value>"
"     <Value>Array_2D</Value>"
"     <Value>Array_2D_Image</Value>"
"     <Value>Array_2D_Map</Value>"
"     <Value>Array_2D_Spectrum</Value>"
"     <Value>Array_3D</Value>"
"     <Value>Array_3D_Image</Value>"
"     <Value>Array_3D_Movie</Value>"
"     <Value>Array_3D_Spectrum</Value>"
"  </Option>"
"  <Option name='BOUNDING_DEGREES' type='string'"
    "description='Manually set bounding box with the syntax "
    "west_lon,south_lat,east_lon,north_lat'/>"
"</CreationOptionList>" );

    poDriver->pfnOpen = PDS4Dataset::Open;
    poDriver->pfnIdentify = PDS4Dataset::Identify;
    poDriver->pfnCreate = PDS4Dataset::Create;
    poDriver->pfnCreateCopy = PDS4Dataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
