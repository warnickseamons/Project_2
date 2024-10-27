#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <climits>
#include <sys/stat.h>

struct Pixel {
    unsigned char r, g, b;
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels;
};

unsigned char clamp(int value) {
    return static_cast<unsigned char>((value > 255) ? 255 : (value < 0) ? 0 : value);
}

float normalize(unsigned char value) {
    return static_cast<float>(value) / 255.0f;
}

unsigned char denormalize(float value) {
    return static_cast<unsigned char>(clamp(static_cast<int>(lroundf(value * 255.0f))));
}

bool fileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

Image readTGA(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        exit(1);
    }

    Image img;
    file.seekg(12);
    file.read(reinterpret_cast<char*>(&img.width), 2);
    file.read(reinterpret_cast<char*>(&img.height), 2);
    img.pixels.resize(img.width * img.height);

    file.seekg(18);
    for (int i = 0; i < img.width * img.height; ++i) {
        file.read(reinterpret_cast<char*>(&img.pixels[i].b), 1);
        file.read(reinterpret_cast<char*>(&img.pixels[i].g), 1);
        file.read(reinterpret_cast<char*>(&img.pixels[i].r), 1);
    }
    file.close();
    return img;
}

void writeTGA(const std::string& filename, const Image& img) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        exit(1);
    }

    char header[18] = {0};
    header[12] = img.width & 0xFF;
    header[13] = (img.width >> 8) & 0xFF;
    header[14] = img.height & 0xFF;
    header[15] = (img.height >> 8) & 0xFF;
    header[16] = 24;

    file.write(header, 18);
    for (const Pixel& p : img.pixels) {
        file.write(reinterpret_cast<const char*>(&p.b), 1);
        file.write(reinterpret_cast<const char*>(&p.g), 1);
        file.write(reinterpret_cast<const char*>(&p.r), 1);
    }
    file.close();
}

Image multiply(const Image& img1, const Image& img2) {
    Image result = img1;
    for (int i = 0; i < img1.width * img1.height; ++i) {
        result.pixels[i].r = denormalize(normalize(img1.pixels[i].r) * normalize(img2.pixels[i].r));
        result.pixels[i].g = denormalize(normalize(img1.pixels[i].g) * normalize(img2.pixels[i].g));
        result.pixels[i].b = denormalize(normalize(img1.pixels[i].b) * normalize(img2.pixels[i].b));
    }
    return result;
}

Image subtract(const Image& img1, const Image& img2) {
    Image result = img1;
    for (int i = 0; i < img1.width * img1.height; ++i) {
        result.pixels[i].r = static_cast<unsigned char>(clamp(static_cast<int>(img1.pixels[i].r) - static_cast<int>(img2.pixels[i].r)));
        result.pixels[i].g = static_cast<unsigned char>(clamp(static_cast<int>(img1.pixels[i].g) - static_cast<int>(img2.pixels[i].g)));
        result.pixels[i].b = static_cast<unsigned char>(clamp(static_cast<int>(img1.pixels[i].b) - static_cast<int>(img2.pixels[i].b)));
    }
    return result;
}

Image screen(const Image& img1, const Image& img2) {
    Image result = img1;
    for (int i = 0; i < img1.width * img1.height; ++i) {
        result.pixels[i].r = denormalize(1 - (1 - normalize(img1.pixels[i].r)) * (1 - normalize(img2.pixels[i].r)));
        result.pixels[i].g = denormalize(1 - (1 - normalize(img1.pixels[i].g)) * (1 - normalize(img2.pixels[i].g)));
        result.pixels[i].b = denormalize(1 - (1 - normalize(img1.pixels[i].b)) * (1 - normalize(img2.pixels[i].b)));
    }
    return result;
}

Image overlay(const Image& img1, const Image& img2) {
    Image result = img1;
    for (int i = 0; i < img1.width * img1.height; ++i) {
        result.pixels[i].r = img2.pixels[i].r <= 128
            ? denormalize(2 * normalize(img1.pixels[i].r) * normalize(img2.pixels[i].r))
            : denormalize(1 - 2 * (1 - normalize(img1.pixels[i].r)) * (1 - normalize(img2.pixels[i].r)));
        result.pixels[i].g = img2.pixels[i].g <= 128
            ? denormalize(2 * normalize(img1.pixels[i].g) * normalize(img2.pixels[i].g))
            : denormalize(1 - 2 * (1 - normalize(img1.pixels[i].g)) * (1 - normalize(img2.pixels[i].g)));
        result.pixels[i].b = img2.pixels[i].b <= 128
            ? denormalize(2 * normalize(img1.pixels[i].b) * normalize(img2.pixels[i].b))
            : denormalize(1 - 2 * (1 - normalize(img1.pixels[i].b)) * (1 - normalize(img2.pixels[i].b)));
    }
    return result;
}

Image adjustGreen(Image& img, int value) {
    for (Pixel& pixel : img.pixels) {
        pixel.g = static_cast<unsigned char>(clamp(static_cast<int>(pixel.g) + value));
    }
    return img;
}

Image scaleRedBlue(Image& img, int redScale, int blueScale) {
    for (Pixel& pixel : img.pixels) {
        pixel.r = static_cast<unsigned char>(clamp(static_cast<int>(pixel.r) * redScale));
        pixel.b = static_cast<unsigned char>(clamp(static_cast<int>(pixel.b) * blueScale));
    }
    return img;
}

void writeChannel(const Image& img, const std::string& filename, char channel) {
    Image result = img;
    for (Pixel& pixel : result.pixels) {
        if (channel == 'r') pixel.g = pixel.b = pixel.r;
        else if (channel == 'g') pixel.r = pixel.b = pixel.g;
        else if (channel == 'b') pixel.r = pixel.g = pixel.b;
    }
    writeTGA(filename, result);
}

Image combineChannels(const Image& red, const Image& green, const Image& blue) {
    Image result = red;
    for (int i = 0; i < red.width * red.height; ++i) {
        result.pixels[i].r = red.pixels[i].r;
        result.pixels[i].g = green.pixels[i].g;
        result.pixels[i].b = blue.pixels[i].b;
    }
    return result;
}

Image rotate180(const Image& img) {
    Image result = img;
    int totalPixels = img.width * img.height;
    for (int i = 0; i < totalPixels; ++i) {
        result.pixels[i] = img.pixels[totalPixels - i - 1];
    }
    return result;
}

int main() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << "Current working directory: " << cwd << std::endl;
    } else {
        std::cerr << "Error getting current working directory" << std::endl;
    }

    std::string filePath = "input/layer1.tga";
    if (!fileExists(filePath)) {
        std::cerr << "File does NOT exist: " << filePath << std::endl;
        return 1;
    }

    Image layer1 = readTGA(filePath);
    Image pattern1 = readTGA("input/pattern1.tga");

    Image result = multiply(layer1, pattern1);
    writeTGA("output/part1.tga", result);

    Image layer2 = readTGA("input/layer2.tga");
    Image car = readTGA("input/car.tga");
    result = subtract(layer2, car);
    writeTGA("output/part2.tga", result);

    Image pattern2 = readTGA("input/pattern2.tga");
    result = multiply(layer1, pattern2);
    Image text = readTGA("input/text.tga");
    result = screen(result, text);
    writeTGA("output/part3.tga", result);

    result = multiply(layer2, readTGA("input/circles.tga"));
    result = subtract(result, pattern2);
    writeTGA("output/part4.tga", result);

    result = overlay(layer1, pattern1);
    writeTGA("output/part5.tga", result);

    result = adjustGreen(car, 200);
    writeTGA("output/part6.tga", result);

    result = scaleRedBlue(car, 4, 0);
    writeTGA("output/part7.tga", result);

    writeChannel(car, "output/part8_r.tga", 'r');
    writeChannel(car, "output/part8_g.tga", 'g');
    writeChannel(car, "output/part8_b.tga", 'b');

    Image red = readTGA("input/layer_red.tga");
    Image green = readTGA("input/layer_green.tga");
    Image blue = readTGA("input/layer_blue.tga");
    result = combineChannels(red, green, blue);
    writeTGA("output/part9.tga", result);

    result = rotate180(readTGA("input/text2.tga"));
    writeTGA("output/part10.tga", result);

    return 0;
}
