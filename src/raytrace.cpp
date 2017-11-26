#include "scene.h"

#include <thread>
#include "ext/yocto_utils.h"

ray3f eval_camera(const camera* cam, const vec2f& uv) {
	auto h = 2 * tan(cam->fovy / 2);
	auto w = h * cam->aspect;
	auto origin = cam->frame.o;
	auto direction = origin + (cam->frame.x * w * (uv.x - 0.5f)) + (cam->frame.y * h * (1 - uv.y - 0.5f)) - cam->frame.z;
	auto l = length(direction - origin);
	return ray3f{ origin, (direction - origin) / l };
}

vec3f lookup_texture(const texture* txt, int i, int j, bool srgb) {
	auto txtcord = txt->ldr.at(i, j);
	auto texcoordf = vec3f{ txtcord.x / 255.0f, txtcord.y / 255.0f, txtcord.z / 255.0f };
	if (srgb)
	{
		texcoordf.x = pow(texcoordf.x, 2.2f);
		texcoordf.y = pow(texcoordf.y, 2.2f);
		texcoordf.z = pow(texcoordf.z, 2.2f);
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
	auto position = vec3f{ 0,0,0 };
	auto norm = vec3f{ 0,0,0 };
	auto texcoord = vec2f{ 0,0 };

	position = eval_pos(shape, inter.ei, inter.ew);
	norm = eval_norm(shape, inter.ei, inter.ew);
	texcoord = eval_texcoord(shape, inter.ei, inter.ew);
	position = transform_point(instance->frame, position);
	norm = transform_direction(instance->frame, norm);

	auto ke = material->ke * eval_texture(material->ke_txt, texcoord, true);
	auto kd = material->kd * eval_texture(material->kd_txt, texcoord, true);
	auto ks = material->ks * eval_texture(material->ks_txt, texcoord, true);
	auto kr = material->kr * eval_texture(material->kr_txt, texcoord, true);
	auto ns = (material->rs) ? 2 / pow(material->rs, 4.0f) - 2 : 1e6f;
	auto color = ke + kd * amb;

	for (auto light : lights)
	{

		if (light->mat->ke.x <= 0.0 && light->mat->ke.y <= 0.0 && light->mat->ke.z <= 0.0) { continue; }
		auto light_pos = transform_point(light->frame, light->shp->pos.front());
		auto l = normalize(light_pos - position);
		auto len = length(light_pos - position);
		auto sr = ray3f{ position, l, 0.01f, len - 0.01f };
		if (intersect_any(scn, sr)) { continue; }

		auto light_I = light->mat->ke / (len * len);
		auto wo = -ray.d;
		auto wh = normalize(l + wo);
		
		if (!shape->triangles.empty())
		{
			auto diff = light_I * kd * max(0.f, dot(norm, l));
			auto blinn = light_I * ks * powf(max(0.f, dot(norm, wh)), ns);
			color += diff + blinn;
		}
		else if (!shape->lines.empty())
		{
			color += (light_I * kd * sqrt(clamp(1 - dot(norm, l) * dot(norm, l), 0.0f, 1.f))) +
				(light_I * ks * pow(sqrt(clamp(1 - dot(norm, wh) * dot(norm, wh), 0.f, 1.f)), ns));
		}
	}
	if (kr != vec3f{ 0,0,0 })
	{
		auto v = ray.o - position;
		auto refray = ray3f{ position, normalize(norm * 2 * dot(v, norm) - v) };
		auto ref = shade(scn, lights, amb, refray);
		color += kr * vec3f{ref.x, ref.y, ref.z};
	}
	return vec4f{ color.x, color.y, color.z, 1.f };
}

image4f raytrace(
    const scene* scn, const vec3f& amb, int resolution, int samples) {
    auto cam = scn->cameras.front();
    auto img = image4f((int)std::round(cam->aspect * resolution), resolution);

	auto lights = std::vector<instance*>();
	for (auto ist : scn->instances)
	{
		if (ist->mat->ke == vec3f{ 0, 0, 0 }) continue;
		if (ist->shp->points.empty()) continue;
		lights.push_back(ist);
	}
	for (int j = 0; j < img.height; ++j)
	{
		for (int i = 0; i < img.width; ++i)
		{
			img.at(i, j) = vec4f{ 0,0,0,0 };
			for (auto sj = 0; sj < samples; ++sj)
			{
				for (auto si = 0; si < samples; ++si)
				{
					auto u = (i + (si + 0.5f) / samples) / img.width;
					auto v = (j + (sj + 0.5f) / samples) / img.height;
					auto ray = eval_camera(cam, { u,v });
					img.at(i, j) += shade(scn, lights, amb, ray);
				}
			}
			auto pixel = img.at(i, j);
			auto ns = samples * samples;
			pixel = vec4f{ pixel.x / ns, pixel.y / ns, pixel.z / ns, pixel.w };
			img.at(i, j) = pixel;
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
