// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include "pdfwriter.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace capypdf {

PdfWriter::PdfWriter(PdfDocument &doc) : doc(doc) {}

rvoe<NoReturnValue> PdfWriter::write_to_file(const std::filesystem::path &ofilename) {
    if(doc.pages.size() == 0) {
        RETERR(NoPages);
    }
    auto tempfname = ofilename;
    tempfname.replace_extension(".pdf~");
    FILE *ofile = fopen(tempfname.string().c_str(), "wb");
    if(!ofile) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }

    try {

        ERCV(doc.write_to_file(ofile));
    } catch(const std::exception &e) {
        fprintf(stderr, "%s\n", e.what());
        fclose(ofile);
        RETERR(DynamicError);
    } catch(...) {
        fprintf(stderr, "Unexpected error.\n");
        fclose(ofile);
        RETERR(DynamicError);
    }

    if(fflush(ofile) != 0) {
        perror(nullptr);
        fclose(ofile);
        RETERR(DynamicError);
    }
    if(
#ifdef _WIN32
        _commit(fileno(ofile))
#else
        fsync(fileno(ofile))
#endif
        != 0) {

        perror(nullptr);
        fclose(ofile);
        RETERR(FileWriteError);
    }
    if(fclose(ofile) != 0) {
        perror(nullptr);
        RETERR(FileWriteError);
    }

    // If we made it here, the file has been fully written and fsynd'd to disk. Now replace.
    std::error_code ec;
    std::filesystem::rename(tempfname, ofilename, ec);
    if(ec) {
        fprintf(stderr, "%s\n", ec.category().message(ec.value()).c_str());
        RETERR(FileWriteError);
    }
    return NoReturnValue{};
}

} // namespace capypdf
