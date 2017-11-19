#include "scene.h"

#include <thread>
#include "ext/yocto_utils.h"

ray3f eval_camera(const camera* cam, const vec2f& uv) {
	auto h = 2 * cam->focus * tan(cam->fovy / 2);
	auto w = h * cam->focus * cam->aspect;
	auto direction = cam->frame.o + cam->frame.x * w * (uv.x - 0.5f) + cam->frame.y * h * (uv.y - 0.5f) - cam->frame.z * cam->focus;
    return ray3f{cam->frame.o, direction};
}

vec3f lookup_texture(const texture* txt, int i, int j, bool srgb) {
	auto txtcord = txt->ldr.at(i, j);
	auto txtcordf = vec3f{ txtcord.x / 255.0f, txtcord.y / 255.0f, txtcord.z / 255.0f };
	if (srgb)
	{
		txtcordf.x = pow(txtcordf.x, 1 / 2.2f);
		txtcordf.y = pow(txtcordf.y, 1 / 2.2f);
		txtcordf.z = pow(txtcordf.z, 1 / 2.2f);
	}
    return txtcordf;
}

vec3f eval_texture(const texture* txt, const vec2f& texcoord, bool srgb) {
    // IL TUO CODICE VA QUI
    return {};
}

vec4f shade(const scene* scn, const std::vector<instance*>& lights,
    const vec3f& amb, const ray3f& ray) {
    // IL TUO CODICE VA QUI
    return {};
}

image4f raytrace(
    const scene* scn, const vec3f& amb, int resolution, int samples) {
    auto cam = scn->cameras.front();
    auto img = image4f((int)std::round(cam->aspect * resolution), resolution);

    // IL TUO CODICE VA QUI

    return img;
}

int main(int argc, char** argv) {
    // command line parsing
    auto parser =
        yu::cmdline::make_parser(argc, argv, "raytrace", "raytrace scene");
    auto resolution = yu::cmdline::parse_opti(
        parser, "--resolution", "-r", "vertical resolution", 720);
    auto samples = yu::cmdline::parse_opti(
        parser, "--samples", "-s", "per-pixel samples", 1);
    auto amb = yu::cmdline::parse_optf(
        parser, "--ambient", "-a", "ambient color", 0.1f);
    auto imageout = yu::cmdline::parse_opts(
        parser, "--output", "-o", "output image", "out.png");
    auto scenein = yu::cmdline::parse_args(
        parser, "scenein", "input scene", "scene.obj", true);
    yu::cmdline::check_parser(parser);

    // load scene
    printf("loading scene %s\n", scenein.c_str());
    auto scn = load_scene(scenein);

    // create bvh
    printf("creating bvh\n");
    build_bvh(scn, false);

    // raytrace
    printf("tracing scene\n");
    auto hdr = raytrace(scn, vec3f{amb, amb, amb}, resolution, samples);

    // tonemap and save
    printf("saving image %s\n", imageout.c_str());
    save_hdr_or_ldr(imageout, hdr);
}
