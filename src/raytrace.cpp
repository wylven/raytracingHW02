#include "scene.h"

#include <thread>
#include "ext/yocto_utils.h"

ray3f eval_camera(const camera* cam, const vec2f& uv) {
	auto h = 2 * tan(cam->fovy / 2);
	auto w = h * cam->aspect;
	auto origin = cam->frame.o;
	auto direction = origin + (cam->frame.x * w * (uv.x - 0.5f)) + (cam->frame.y * h * (uv.y - 0.5f)) - cam->frame.z;
	auto direction2 = direction - origin;
	auto length = sqrt(direction2.x*direction2.x + direction2.y*direction2.y + direction2.z*direction2.z);
	return ray3f{ origin, direction2 / length };
}

vec3f lookup_texture(const texture* txt, int i, int j, bool srgb) {
	auto txtcord = txt->ldr.at(i, j);
	auto texcoordf = vec3f{ txtcord.x / 255.0f, txtcord.y / 255.0f, txtcord.z / 255.0f };
	if (srgb)
	{
		texcoordf.x = pow(texcoordf.x, 1 / 2.2f);
		texcoordf.y = pow(texcoordf.y, 1 / 2.2f);
		texcoordf.z = pow(texcoordf.z, 1 / 2.2f);
	}
	return texcoordf;
}

vec3f eval_texture(const texture* txt, const vec2f& texcoord, bool srgb) {
    // IL TUO CODICE VA QUI
	if (!txt) { return { 1, 1, 1 }; }
	auto s = fmod(texcoord.x, 1.f) * txt->ldr.width;
	auto t = fmod(texcoord.y, 1.f) * txt->ldr.height;
	if (s < 0) { s = txt->ldr.width; }
	if (t < 0) { t = txt->ldr.height; }
	auto i = floor(s);
	auto j = floor(t);
	auto i2 = fmod(i + 1.f, (float)txt->ldr.width);
	auto j2 = fmod(j + 1.f, (float)txt->ldr.height);
	auto wi = s - i;
	auto wj = t - j;
	auto pixelij = lookup_texture(txt, (int)i, (int)j, true) * (1.f - wi) * (1.f - wj);
	auto pixeli2j = lookup_texture(txt, (int)i2, (int)j, true) * wi * (1.f - wj);
	auto pixelij2 = lookup_texture(txt, (int)i, (int)j2, true) * wj * (1.f - wi);
	auto pixeli2j2 = lookup_texture(txt, (int)i2, (int)j2, true) * wi * wj;

	return pixelij + pixeli2j + pixelij2 + pixeli2j2;
}

vec4f shade(const scene* scn, const std::vector<instance*>& lights,
    const vec3f& amb, const ray3f& ray) {

	if (!intersect_any(scn, ray)) { return vec4f{ 0, 0, 0, 0 }; }
	auto inter = intersect_first(scn, ray);
	auto instance = inter.ist;
	auto shape = instance->shp;
	auto material = instance->mat;

	auto position = eval_pos(shape, inter.ei, inter.ew);
	auto normal = eval_norm(shape, inter.ei, inter.ew);
	auto texcoord = eval_texcoord(shape, inter.ei, inter.ew);
	auto kd = material->kd * eval_texture(material->kd_txt, texcoord, true);
	auto colour = vec4f{ kd.x, kd.y, kd.z, 1.f };
	return colour;
}

image4f raytrace(
    const scene* scn, const vec3f& amb, int resolution, int samples) {
    auto cam = scn->cameras.front();
    auto img = image4f((int)std::round(cam->aspect * resolution), resolution);

	for (int j = 0; j < img.height; ++j)
	{
		for (int i = 0; i < img.width; ++i)
		{
			auto uv = vec2f{(float)i / img.width, (float)j / img.height};
			auto ray = eval_camera(cam, uv);
			img.at(i, j) = shade(scn, scn->instances, amb, ray);
		}
	}
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
