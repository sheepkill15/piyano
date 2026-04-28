#pragma once

class PatchDriver;

const PatchDriver* subtractivePatchDriverPtr() noexcept;
const PatchDriver* fmPatchDriverPtr() noexcept;
const PatchDriver* pianoPatchDriverPtr() noexcept;
const PatchDriver* ePianoPatchDriverPtr() noexcept;
const PatchDriver* stringsPatchDriverPtr() noexcept;
const PatchDriver* bassPatchDriverPtr() noexcept;
const PatchDriver* drumsPatchDriverPtr() noexcept;
