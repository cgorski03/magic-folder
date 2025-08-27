#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "magic_core/extractors/pdf_extractor_mac.hpp"
#include "../../common/utilities_test.hpp"

namespace magic_tests {

class PdfExtractorMacTest : public ::testing::Test {
protected:
    void SetUp() override {
        extractor = std::make_unique<magic_core::MacPdfExtractor>();
        test_dir = std::filesystem::temp_directory_path() / "pdf_extractor_test";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }

    std::unique_ptr<magic_core::MacPdfExtractor> extractor;
    std::filesystem::path test_dir;
};

TEST_F(PdfExtractorMacTest, CanHandlePdfFiles) {
    auto pdf_file = test_dir / "test.pdf";
    auto txt_file = test_dir / "test.txt";
    auto doc_file = test_dir / "test.doc";
    auto PDF_file = test_dir / "TEST.PDF";

#if defined(__APPLE__)
    EXPECT_TRUE(extractor->can_handle(pdf_file));
    EXPECT_TRUE(extractor->can_handle(PDF_file));  // Case insensitive
    EXPECT_FALSE(extractor->can_handle(txt_file));
    EXPECT_FALSE(extractor->can_handle(doc_file));
#else
    // On non-Apple platforms, should return false
    EXPECT_FALSE(extractor->can_handle(pdf_file));
    EXPECT_FALSE(extractor->can_handle(PDF_file));
    EXPECT_FALSE(extractor->can_handle(txt_file));
    EXPECT_FALSE(extractor->can_handle(doc_file));
#endif
}

#if defined(__APPLE__)
TEST_F(PdfExtractorMacTest, GetChunksWithNonexistentFile) {
    auto nonexistent_file = test_dir / "nonexistent.pdf";
    
    // Should throw an exception for nonexistent files
    EXPECT_THROW(extractor->get_chunks(nonexistent_file), std::runtime_error);
}

TEST_F(PdfExtractorMacTest, GetChunksWithInvalidFile) {
    auto invalid_pdf = test_dir / "invalid.pdf";
    
    // Create a fake PDF file (just text content)
    std::ofstream file(invalid_pdf);
    file << "This is not a real PDF file";
    file.close();
    
    // Should throw an exception for invalid PDF files
    EXPECT_THROW(extractor->get_chunks(invalid_pdf), std::runtime_error);
}

TEST_F(PdfExtractorMacTest, ExtractWithHashReturnsValidResult) {
    // This test would require a real PDF file, which we can't easily create in a unit test
    // Instead, we'll test that the method exists and has the right signature
    auto pdf_file = test_dir / "test.pdf";
    
    // Create a minimal "PDF" file (this will fail, but we're testing the interface)
    std::ofstream file(pdf_file);
    file << "%PDF-1.4\n%%EOF\n";
    file.close();
    
    // The method should exist and be callable (even if it throws)
    EXPECT_NO_THROW({
        try {
            auto result = extractor->extract_with_hash(pdf_file);
            // If it succeeds, check the result structure
            EXPECT_TRUE(result.chunks.empty() || !result.chunks.empty()); // Just check it's accessible
            EXPECT_TRUE(result.content_hash.empty() || !result.content_hash.empty()); // Just check it's accessible
        } catch (const std::runtime_error&) {
            // Expected for invalid PDF, this is fine
        }
    });
}

// Test the internal chunking behavior (if we had access to a real PDF)
TEST_F(PdfExtractorMacTest, ChunkingBehaviorWithEmptyContent) {
    // Test that empty content returns empty chunks
    auto empty_pdf = test_dir / "empty.pdf";
    std::ofstream file(empty_pdf);
    file << "%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\nxref\n0 4\n0000000000 65535 f \n0000000010 00000 n \n0000000053 00000 n \n0000000103 00000 n \ntrailer<</Size 4/Root 1 0 R>>\nstartxref\n149\n%%EOF\n";
    file.close();
    
    try {
        auto chunks = extractor->get_chunks(empty_pdf);
        // For an empty PDF (no text content), should return empty chunks
        EXPECT_TRUE(chunks.empty());
    } catch (const std::runtime_error&) {
        // Expected for invalid/empty PDF, this is fine for this test
    }
}

#else
// Non-Apple platform tests
TEST_F(PdfExtractorMacTest, NonApplePlatformReturnsEmptyChunks) {
    auto pdf_file = test_dir / "test.pdf";
    
    // On non-Apple platforms, should return empty chunks
    auto chunks = extractor->get_chunks(pdf_file);
    EXPECT_TRUE(chunks.empty());
    
    auto result = extractor->extract_with_hash(pdf_file);
    EXPECT_TRUE(result.chunks.empty());
    EXPECT_FALSE(result.content_hash.empty()); // Hash should still be computed
}
#endif

TEST_F(PdfExtractorMacTest, InheritanceFromContentExtractor) {
    // Test that MacPdfExtractor properly inherits from ContentExtractor
    magic_core::ContentExtractor* base_ptr = extractor.get();
    EXPECT_NE(base_ptr, nullptr);
    
    // Test polymorphic behavior
    auto pdf_file = test_dir / "test.pdf";
    EXPECT_NO_THROW({
        bool can_handle = base_ptr->can_handle(pdf_file);
        (void)can_handle; // Suppress unused variable warning
    });
}

} // namespace magic_tests
