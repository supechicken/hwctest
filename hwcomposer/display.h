#pragma once
#include "fb_sink.h"

struct display {
    int width {1280};
    int height {800};
    int refresh {60};
    FbSink* sink {nullptr};
};
