#include <iostream>
#include <emscripten.h>
#include <cstring>
#include <cmath>
#include "Sound.h"
#include "Spectrum.h"
#include "Spectrogram.h"
#include "Pitch.h"
#include "Sound_and_Spectrum.h"
#include "Sound_and_Spectrogram.h"
#include "Sound_to_Pitch.h"
#include "melder.h"

// EMSCRIPTEN_KEEPALIVE 防止函数被优化掉

// 全局变量用于存储当前加载的音频
static structSound* g_currentSound = nullptr;
static autoSound g_autoSound;

// 初始化 Praat 库
extern "C" EMSCRIPTEN_KEEPALIVE
void init_praat() {
    Melder_init();
    std::cout << "Praat initialized" << std::endl;
}

// 从内存数据创建 Sound（适合 WASM 环境）
extern "C" EMSCRIPTEN_KEEPALIVE
int load_sound_from_memory(float* samples, int numSamples, float sampleRate) {
    try {
        // 创建新的 Sound 对象（单声道）
        autoSound sound = Sound_create(1, 0.0, (double)numSamples / sampleRate, numSamples, 1.0 / sampleRate, 0.0);

        if (!sound) {
            std::cerr << "Failed to create Sound object" << std::endl;
            return 0;
        }

        // 填充采样数据
        for (integer i = 1; i <= numSamples; ++i) {
            sound->z[1][i] = samples[i - 1];
        }

        // 保存到全局变量（使用移动语义）
        g_autoSound = std::move(sound);
        g_currentSound = g_autoSound.get();

        std::cout << "Sound loaded from memory: " << numSamples << " samples at " << sampleRate << " Hz" << std::endl;
        return 1;

    } catch (const MelderError& e) {
        std::cerr << "Praat error: " << Melder_peek32to8(Melder_getError()) << std::endl;
        Melder_clearError();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 0;
    } catch (...) {
        std::cerr << "Unknown error" << std::endl;
        return 0;
    }
}

// 加载音频文件并返回信息
extern "C" EMSCRIPTEN_KEEPALIVE
const char* load_sound(const char* filePath) {
    static std::string result;

    try {
        structMelderFile fileStruct = { };
        MelderFile file = &fileStruct;
        Melder_pathToFile(Melder_peek8to32(filePath), file);

        autoSound sound = Sound_readFromSoundFile(file);

        if (!sound) {
            result = "Error: Failed to load sound";
            return result.c_str();
        }

        double sampleRate = 1.0 / sound->dx;
        int channels = sound->ny;
        int samples = sound->nx;
        double duration = sound->xmax - sound->xmin;

        // 格式化返回信息
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 "Sound loaded successfully!\n"
                 "Sample Rate: %.0f Hz\n"
                 "Channels: %d\n"
                 "Samples: %d\n"
                 "Duration: %.5f seconds",
                 sampleRate, channels, samples, duration);

        result = buffer;
        return result.c_str();

    } catch (const MelderError& e) {
        result = std::string("Praat error: ") + Melder_peek32to8(Melder_getError());
        return result.c_str();
    } catch (const std::exception& e) {
        result = std::string("Error: ") + e.what();
        return result.c_str();
    } catch (...) {
        result = "Unknown error";
        return result.c_str();
    }
}

// 获取音频采样数据
extern "C" EMSCRIPTEN_KEEPALIVE
void get_sound_samples(float* outputBuffer, int* numSamples) {
    try {
        if (!g_autoSound) {
            *numSamples = 0;
            return;
        }

        *numSamples = (int)g_autoSound->nx;

        // 复制采样数据到输出缓冲区
        for (integer i = 1; i <= g_autoSound->nx; ++i) {
            outputBuffer[i - 1] = (float)g_autoSound->z[1][i];
        }

    } catch (...) {
        *numSamples = 0;
    }
}

// 获取音频时间轴数据
extern "C" EMSCRIPTEN_KEEPALIVE
void get_sound_time_axis(float* timeBuffer, int* numSamples) {
    try {
        if (!g_autoSound) {
            *numSamples = 0;
            return;
        }

        *numSamples = (int)g_autoSound->nx;

        // 复制时间轴数据
        double dx = g_autoSound->dx;
        double xmin = g_autoSound->xmin;
        for (integer i = 0; i < g_autoSound->nx; ++i) {
            timeBuffer[i] = (float)(xmin + i * dx);
        }

    } catch (...) {
        *numSamples = 0;
    }
}

// 获取音频基本信息（采样率、通道数、采样数、时长）
extern "C" EMSCRIPTEN_KEEPALIVE
void get_sound_info(float* sampleRate, int* channels, int* samples, float* duration) {
    try {
        if (!g_autoSound) {
            *sampleRate = 0;
            *channels = 0;
            *samples = 0;
            *duration = 0;
            return;
        }

        *sampleRate = (float)(1.0 / g_autoSound->dx);
        *channels = (int)g_autoSound->ny;
        *samples = (int)g_autoSound->nx;
        *duration = (float)(g_autoSound->xmax - g_autoSound->xmin);

    } catch (...) {
        *sampleRate = 0;
        *channels = 0;
        *samples = 0;
        *duration = 0;
    }
}

// 获取频谱数据（频率和振幅）
extern "C" EMSCRIPTEN_KEEPALIVE
void get_spectrum(float* frequencies, float* amplitudes, int* numBins) {
    *numBins = 0; // 默认设置为0，出错时返回0

    try {
        if (!g_autoSound) {
            return;
        }

        // 使用 FFT (fast = true) 进行频谱计算，numberOfFrequencies = numberOfFourierSamples / 2 + 1
        autoSpectrum spectrum = Sound_to_Spectrum(g_autoSound.get(), true);

        if (!spectrum) {
            return;
        }

        // 获取实际的频谱点数
        *numBins = (int)spectrum->nx;

        // 提取频率和振幅数据
        for (integer i = 1; i <= spectrum->nx; ++i) {
            // 频率计算: x1 + (i-1) * dx
            double freq = spectrum->x1 + (i - 1) * spectrum->dx;
            double real = spectrum->z[1][i];
            double imag = spectrum->z[2][i];
            double amplitude = std::sqrt(real * real + imag * imag);

            frequencies[i - 1] = (float)freq;
            amplitudes[i - 1] = (float)amplitude;
        }

    } catch (...) {
        *numBins = 0;
    }
}

// 获取频谱图数据（Spectrogram）
extern "C" EMSCRIPTEN_KEEPALIVE
void get_spectrogram(float* times, float* frequencies, float* values, int* numTimes, int* numFreqs) {
    *numTimes = 0;
    *numFreqs = 0;

    try {
        if (!g_autoSound) {
            return;
        }

        // 创建频谱图 (window_length=0.03, maximum_frequency=8000)
        autoSpectrogram spectrogram = Sound_to_Spectrogram(
            g_autoSound.get(),
            0.03,  // window_length (effectiveAnalysisWidth)
            8000.0,  // maximum_frequency (fmax)
            0.0,  // minimumTimeStep
            0.0,  // minimumFreqStep
            kSound_to_Spectrogram_windowShape::GAUSSIAN,
            1.0,  // maximumTimeOversampling
            1.0   // maximumFreqOversampling
        );

        if (!spectrogram) {
            return;
        }

        *numTimes = (int)spectrogram->nx;
        *numFreqs = (int)spectrogram->ny;

        // 提取时间轴
        for (integer i = 0; i < spectrogram->nx; ++i) {
            times[i] = (float)(spectrogram->x1 + i * spectrogram->dx);
        }

        // 提取频率轴
        for (integer i = 0; i < spectrogram->ny; ++i) {
            frequencies[i] = (float)(spectrogram->y1 + i * spectrogram->dy);
        }

        // 提取频谱图数值 (z [iy] [ix] -> values[i][j])
        for (integer ix = 0; ix < spectrogram->nx; ++ix) {
            for (integer iy = 0; iy < spectrogram->ny; ++iy) {
                // Praat 使用 1-based 索引
                values[iy * spectrogram->nx + ix] = (float)spectrogram->z[iy + 1][ix + 1];
            }
        }

    } catch (...) {
        *numTimes = 0;
        *numFreqs = 0;
    }
}

// 获取音高数据
extern "C" EMSCRIPTEN_KEEPALIVE
void get_pitch(float* times, float* values, int* numFrames) {
    *numFrames = 0;

    try {
        if (!g_autoSound) {
            return;
        }

        // 使用 autocorrelation 方法提取音高
        // pitchFloor: 75 Hz, pitchCeiling: 600 Hz
        autoPitch pitch = Sound_to_Pitch(
            g_autoSound.get(),
            0.01,      // timeStep (10ms)
            75.0,       // pitchFloor
            600.0        // pitchCeiling
        );

        if (!pitch) {
            return;
        }

        *numFrames = (int)pitch->nx;

        // 提取时间轴和音高值
        for (integer i = 0; i < pitch->nx; ++i) {
            times[i] = (float)(pitch->x1 + i * pitch->dx);
            // 获取第一个候选的频率（如果未检测到音高，频率为0）
            if (pitch->frames[i + 1].nCandidates > 0) {
                values[i] = (float)pitch->frames[i + 1].candidates[1].frequency;
            } else {
                values[i] = 0.0f;
            }
        }

    } catch (...) {
        *numFrames = 0;
    }
}

// 主函数
int main() {
    std::cout << "Praat WebAssembly Module Loaded" << std::endl;
    std::cout << "Call init_praat() to initialize" << std::endl;
    std::cout << "Call load_sound_from_memory(samples, numSamples, sampleRate) to load audio" << std::endl;
    return 0;
}
