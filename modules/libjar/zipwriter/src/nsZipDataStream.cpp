/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "StreamFunctions.h"
#include "nsZipDataStream.h"
#include "nsIStringStream.h"
#include "nsISeekableStream.h"
#include "nsDeflateConverter.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"

#define ZIP_METHOD_STORE 0
#define ZIP_METHOD_DEFLATE 8

/**
 * nsZipDataStream handles the writing an entry's into the zip file.
 * It is set up to wither write the data as is, or in the event that compression
 * has been requested to pass it through a stream converter.
 * Currently only the deflate compression method is supported.
 * The CRC checksum for the entry's data is also generated here.
 */
NS_IMPL_THREADSAFE_ISUPPORTS2(nsZipDataStream, nsIStreamListener,
                                               nsIRequestObserver)

nsresult nsZipDataStream::Init(nsZipWriter *aWriter,
                               nsIOutputStream *aStream,
                               nsZipHeader *aHeader,
                               PRInt32 aCompression)
{
    mWriter = aWriter;
    mHeader = aHeader;
    mStream = aStream;
    mHeader->mCRC = crc32(0L, Z_NULL, 0);

    nsresult rv = NS_NewSimpleStreamListener(getter_AddRefs(mOutput), aStream,
                                             nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aCompression > 0) {
        mHeader->mMethod = ZIP_METHOD_DEFLATE;
        nsCOMPtr<nsIStreamConverter> converter =
                              new nsDeflateConverter(aCompression);
        NS_ENSURE_TRUE(converter, NS_ERROR_OUT_OF_MEMORY);

        rv = converter->AsyncConvertData("uncompressed", "rawdeflate", mOutput,
                                         nsnull);
        NS_ENSURE_SUCCESS(rv, rv);

        mOutput = do_QueryInterface(converter, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        mHeader->mMethod = ZIP_METHOD_STORE;
    }

    return NS_OK;
}

/* void onDataAvailable (in nsIRequest aRequest, in nsISupports aContext,
 *                       in nsIInputStream aInputStream,
 *                       in unsigned long aOffset, in unsigned long aCount); */
NS_IMETHODIMP nsZipDataStream::OnDataAvailable(nsIRequest *aRequest,
                                               nsISupports *aContext,
                                               nsIInputStream *aInputStream,
                                               PRUint32 aOffset,
                                               PRUint32 aCount)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsAutoArrayPtr<char> buffer(new char[aCount]);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = ZW_ReadData(aInputStream, buffer.get(), aCount);
    NS_ENSURE_SUCCESS(rv, rv);

    return ProcessData(aRequest, aContext, buffer.get(), aOffset, aCount);
}

/* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
NS_IMETHODIMP nsZipDataStream::OnStartRequest(nsIRequest *aRequest,
                                              nsISupports *aContext)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    return mOutput->OnStartRequest(aRequest, aContext);
}

/* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext,
 *                     in nsresult aStatusCode); */
NS_IMETHODIMP nsZipDataStream::OnStopRequest(nsIRequest *aRequest,
                                             nsISupports *aContext,
                                             nsresult aStatusCode)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = mOutput->OnStopRequest(aRequest, aContext, aStatusCode);
    mOutput = nsnull;
    if (NS_FAILED(rv)) {
        mWriter->EntryCompleteCallback(mHeader, rv);
    }
    else {
        rv = CompleteEntry();
        rv = mWriter->EntryCompleteCallback(mHeader, rv);
    }

    mStream = nsnull;
    mWriter = nsnull;
    mHeader = nsnull;

    return rv;
}

inline nsresult nsZipDataStream::CompleteEntry()
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mStream, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 pos;
    rv = seekable->Tell(&pos);
    NS_ENSURE_SUCCESS(rv, rv);

    mHeader->mCSize = pos - mHeader->mOffset - mHeader->GetFileHeaderLength();
    mHeader->mWriteOnClose = true;
    return NS_OK;
}

nsresult nsZipDataStream::ProcessData(nsIRequest *aRequest,
                                      nsISupports *aContext, char *aBuffer,
                                      PRUint32 aOffset, PRUint32 aCount)
{
    mHeader->mCRC = crc32(mHeader->mCRC,
                          reinterpret_cast<const unsigned char*>(aBuffer),
                          aCount);

    nsresult rv;
    nsCOMPtr<nsIStringInputStream> stream =
             do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    stream->ShareData(aBuffer, aCount);
    rv = mOutput->OnDataAvailable(aRequest, aContext, stream, aOffset, aCount);
    mHeader->mUSize += aCount;

    return rv;
}

nsresult nsZipDataStream::ReadStream(nsIInputStream *aStream)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = OnStartRequest(nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoArrayPtr<char> buffer(new char[4096]);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 read = 0;
    PRUint32 offset = 0;
    do
    {
        rv = aStream->Read(buffer.get(), 4096, &read);
        if (NS_FAILED(rv)) {
            OnStopRequest(nsnull, nsnull, rv);
            return rv;
        }

        if (read > 0) {
            rv = ProcessData(nsnull, nsnull, buffer.get(), offset, read);
            if (NS_FAILED(rv)) {
                OnStopRequest(nsnull, nsnull, rv);
                return rv;
            }
            offset += read;
        }
    } while (read > 0);

    return OnStopRequest(nsnull, nsnull, NS_OK);
}
