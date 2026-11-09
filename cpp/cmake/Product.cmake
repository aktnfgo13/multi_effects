# Keep host identifiers stable across releases; branding is independent of identity.
set(MULTI_EFFECTS_PRODUCT_NAME "Multi Effects")
set(MULTI_EFFECTS_PRODUCT_VERSION "0.8.0")
set(MULTI_EFFECTS_PRODUCT_STAGE "Alpha")
configure_file(${CMAKE_CURRENT_LIST_DIR}/ProductInfo.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ProductInfo.h @ONLY)
