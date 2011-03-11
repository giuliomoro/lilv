/*
  Copyright 2007-2011 David Robillard <http://drobilla.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _XOPEN_SOURCE 500

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <wordexp.h>
#ifdef SLV2_DYN_MANIFEST
#include <dlfcn.h>
#endif

#include "slv2_internal.h"

static void
slv2_world_set_prefix(SLV2World world, const char* name, const char* uri)
{
	const SerdNode name_node = serd_node_from_string(SERD_LITERAL,
	                                                 (const uint8_t*)name);
	const SerdNode uri_node  = serd_node_from_string(SERD_URI,
	                                                 (const uint8_t*)uri);
	serd_env_add(world->namespaces, &name_node, &uri_node);
}

SLV2_API
SLV2World
slv2_world_new()
{
	SLV2World world = (SLV2World)malloc(sizeof(struct _SLV2World));

	world->world = sord_world_new();
	if (!world->world)
		goto fail;

	world->model = sord_new(world->world, SORD_SPO|SORD_OPS, true);
	if (!world->model)
		goto fail;

	world->plugin_classes = slv2_plugin_classes_new();
	world->plugins        = slv2_plugins_new();

#define NS_DYNMAN (const uint8_t*)"http://lv2plug.in/ns/ext/dynmanifest#"
#define NS_DC     (const uint8_t*)"http://dublincore.org/documents/dcmi-namespace/"

#define NEW_URI(uri)     sord_new_uri(world->world, uri)
#define NEW_URI_VAL(uri) slv2_value_new_from_node( \
		world, sord_new_uri(world->world, uri));

	world->dc_replaces_node        = NEW_URI(NS_DC        "replaces");
	world->dyn_manifest_node       = NEW_URI(NS_DYNMAN    "DynManifest");
	world->lv2_specification_node  = NEW_URI(SLV2_NS_LV2  "Specification");
	world->lv2_plugin_node         = NEW_URI(SLV2_NS_LV2  "Plugin");
	world->lv2_binary_node         = NEW_URI(SLV2_NS_LV2  "binary");
	world->lv2_default_node        = NEW_URI(SLV2_NS_LV2  "default");
	world->lv2_minimum_node        = NEW_URI(SLV2_NS_LV2  "minimum");
	world->lv2_maximum_node        = NEW_URI(SLV2_NS_LV2  "maximum");
	world->lv2_port_node           = NEW_URI(SLV2_NS_LV2  "port");
	world->lv2_portproperty_node   = NEW_URI(SLV2_NS_LV2  "portProperty");
	world->lv2_reportslatency_node = NEW_URI(SLV2_NS_LV2  "reportsLatency");
	world->lv2_index_node          = NEW_URI(SLV2_NS_LV2  "index");
	world->lv2_symbol_node         = NEW_URI(SLV2_NS_LV2  "symbol");
	world->rdf_a_node              = NEW_URI(SLV2_NS_RDF  "type");
	world->rdf_value_node          = NEW_URI(SLV2_NS_RDF  "value");
	world->rdfs_class_node         = NEW_URI(SLV2_NS_RDFS "Class");
	world->rdfs_label_node         = NEW_URI(SLV2_NS_RDFS "label");
	world->rdfs_seealso_node       = NEW_URI(SLV2_NS_RDFS "seeAlso");
	world->rdfs_subclassof_node    = NEW_URI(SLV2_NS_RDFS "subClassOf");
	world->slv2_bundleuri_node     = NEW_URI(SLV2_NS_SLV2 "bundleURI");
	world->slv2_dmanifest_node     = NEW_URI(SLV2_NS_SLV2 "dynamic-manifest");
	world->xsd_boolean_node        = NEW_URI(SLV2_NS_XSD  "boolean");
	world->xsd_decimal_node        = NEW_URI(SLV2_NS_XSD  "decimal");
	world->xsd_double_node         = NEW_URI(SLV2_NS_XSD  "double");
	world->xsd_integer_node        = NEW_URI(SLV2_NS_XSD  "integer");

	world->doap_name_val = NEW_URI_VAL(SLV2_NS_DOAP "name");
	world->lv2_name_val  = NEW_URI_VAL(SLV2_NS_LV2  "name");

	world->lv2_plugin_class = slv2_plugin_class_new(
		world, NULL, world->lv2_plugin_node, "Plugin");
	assert(world->lv2_plugin_class);

	world->namespaces = serd_env_new();
	slv2_world_set_prefix(world, "rdf",   "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	slv2_world_set_prefix(world, "rdfs",  "http://www.w3.org/2000/01/rdf-schema#");
	slv2_world_set_prefix(world, "doap",  "http://usefulinc.com/ns/doap#");
	slv2_world_set_prefix(world, "foaf",  "http://xmlns.com/foaf/0.1/");
	slv2_world_set_prefix(world, "lv2",   "http://lv2plug.in/ns/lv2core#");
	slv2_world_set_prefix(world, "lv2ev", "http://lv2plug.in/ns/ext/event#");

	world->n_read_files    = 0;
	world->filter_language = true;

	return world;

fail:
	/* keep on rockin' in the */ free(world);
	return NULL;
}

SLV2_API
void
slv2_world_free(SLV2World world)
{
	slv2_plugin_class_free(world->lv2_plugin_class);
	world->lv2_plugin_class = NULL;

	slv2_node_free(world, world->dc_replaces_node);
	slv2_node_free(world, world->dyn_manifest_node);
	slv2_node_free(world, world->lv2_specification_node);
	slv2_node_free(world, world->lv2_plugin_node);
	slv2_node_free(world, world->lv2_binary_node);
	slv2_node_free(world, world->lv2_default_node);
	slv2_node_free(world, world->lv2_minimum_node);
	slv2_node_free(world, world->lv2_maximum_node);
	slv2_node_free(world, world->lv2_port_node);
	slv2_node_free(world, world->lv2_portproperty_node);
	slv2_node_free(world, world->lv2_reportslatency_node);
	slv2_node_free(world, world->lv2_index_node);
	slv2_node_free(world, world->lv2_symbol_node);
	slv2_node_free(world, world->rdf_a_node);
	slv2_node_free(world, world->rdf_value_node);
	slv2_node_free(world, world->rdfs_label_node);
	slv2_node_free(world, world->rdfs_seealso_node);
	slv2_node_free(world, world->rdfs_subclassof_node);
	slv2_node_free(world, world->rdfs_class_node);
	slv2_node_free(world, world->slv2_bundleuri_node);
	slv2_node_free(world, world->slv2_dmanifest_node);
	slv2_node_free(world, world->xsd_boolean_node);
	slv2_node_free(world, world->xsd_decimal_node);
	slv2_node_free(world, world->xsd_double_node);
	slv2_node_free(world, world->xsd_integer_node);
	slv2_value_free(world->doap_name_val);
	slv2_value_free(world->lv2_name_val);

	SLV2_FOREACH(i, world->plugins) {
		SLV2Plugin p = slv2_plugins_get(world->plugins, i);
		slv2_plugin_free(p);
	}
	g_sequence_free(world->plugins);
	world->plugins = NULL;

	g_sequence_free(world->plugin_classes);
	world->plugin_classes = NULL;

	sord_free(world->model);
	world->model = NULL;

	sord_world_free(world->world);
	world->world = NULL;

	serd_env_free(world->namespaces);

	free(world);
}

SLV2_API
void
slv2_world_set_option(SLV2World       world,
                      const char*     option,
                      const SLV2Value value)
{
	if (!strcmp(option, SLV2_OPTION_FILTER_LANG)) {
		if (slv2_value_is_bool(value)) {
			world->filter_language = slv2_value_as_bool(value);
			return;
		}
	} else {
		SLV2_WARNF("Unrecognized or invalid option `%s'\n", option);
	}
}

static SLV2Matches
slv2_world_find_statements(SLV2World world,
                           SordModel model,
                           SLV2Node  subject,
                           SLV2Node  predicate,
                           SLV2Node  object,
                           SLV2Node  graph)
{
	SordQuad pat = { subject, predicate, object, graph };
	return sord_find(model, pat);
}

static SerdNode
slv2_new_uri_relative_to_base(const uint8_t* uri_str, const uint8_t* base_uri_str)
{
	SerdURI base_uri;
	if (!serd_uri_parse(base_uri_str, &base_uri)) {
		return SERD_NODE_NULL;
	}

	SerdURI ignored;
	return serd_node_new_uri_from_string(uri_str, &base_uri, &ignored);
}

const uint8_t*
slv2_world_blank_node_prefix(SLV2World world)
{
	static char str[32];
	snprintf(str, sizeof(str), "%d", world->n_read_files++);
	return (const uint8_t*)str;
}

/** Comparator for sequences (e.g. world->plugins). */
int
slv2_header_compare_by_uri(const void* a, const void* b, void* user_data)
{
	const struct _SLV2Header* const header_a = (const struct _SLV2Header*)a;
	const struct _SLV2Header* const header_b = (const struct _SLV2Header*)b;
	return strcmp(slv2_value_as_uri(header_a->uri),
	              slv2_value_as_uri(header_b->uri));
}

/** Get an element of a sequence of any object with an SLV2Header by URI. */
struct _SLV2Header*
slv2_sequence_get_by_uri(GSequence* seq,
                         SLV2Value  uri)
{
	struct _SLV2Header key = { NULL, uri }; 
	GSequenceIter*     i   = g_sequence_search(
		seq, &key, slv2_header_compare_by_uri, NULL);

	// i points to where plugin would be inserted (not necessarily a match)

	if (!g_sequence_iter_is_end(i)) {
		SLV2Plugin p = g_sequence_get(i);
		if (slv2_value_equals(slv2_plugin_get_uri(p), uri)) {
			return (struct _SLV2Header*)p;
		}
	}

	if (!g_sequence_iter_is_begin(i)) {
		// Check if i is just past a match
		i = g_sequence_iter_prev(i);
		SLV2Plugin p = g_sequence_get(i);
		if (slv2_value_equals(slv2_plugin_get_uri(p), uri)) {
			return (struct _SLV2Header*)p;
		}
	}

	return NULL;
}

static void
slv2_world_add_plugin(SLV2World world,
                      SLV2Node  plugin_node,
                      SerdNode* manifest_uri,
                      SLV2Node  dyn_manifest_lib,
                      SLV2Node  bundle_node)
{
	SLV2Value plugin_uri  = slv2_value_new_from_node(world, plugin_node);

	SLV2Plugin last = slv2_plugins_get_by_uri(world->plugins, plugin_uri);
	if (last) {
		SLV2_ERRORF("Duplicate plugin <%s>\n", slv2_value_as_uri(plugin_uri));
		SLV2_ERRORF("... found in %s\n", slv2_value_as_string(
			            slv2_plugin_get_bundle_uri(last)));
		SLV2_ERRORF("... and      %s\n", sord_node_get_string(bundle_node));
		slv2_value_free(plugin_uri);
		return;
	}

	// Create SLV2Plugin
	SLV2Value  bundle_uri = slv2_value_new_from_node(world, bundle_node);
	SLV2Plugin plugin     = slv2_plugin_new(world, plugin_uri, bundle_uri);

	// Add manifest as plugin data file (as if it were rdfs:seeAlso)
	slv2_array_append(plugin->data_uris,
	                  slv2_value_new_uri(world, (const char*)manifest_uri->buf));

	// Set dynamic manifest library URI, if applicable
	if (dyn_manifest_lib) {
		plugin->dynman_uri = slv2_value_new_from_node(world, dyn_manifest_lib);
	}
		
	// Add all plugin data files (rdfs:seeAlso)
	SLV2Matches files = slv2_world_find_statements(
		world, world->model,
		plugin_node,
		world->rdfs_seealso_node,
		NULL,
		NULL);
	FOREACH_MATCH(files) {
		SLV2Node file_node = slv2_match_object(files);
		slv2_array_append(plugin->data_uris,
		                  slv2_value_new_from_node(world, file_node));
	}
	slv2_match_end(files);

	// Add plugin to world plugin sequence
	slv2_sequence_insert(world->plugins, plugin);
}

SLV2_API
void
slv2_world_load_bundle(SLV2World world, SLV2Value bundle_uri)
{
	if (!slv2_value_is_uri(bundle_uri)) {
		SLV2_ERROR("Bundle 'URI' is not a URI\n");
		return;
	}

	const SordNode bundle_node = bundle_uri->val.uri_val;

	SerdNode manifest_uri = slv2_new_uri_relative_to_base(
		(const uint8_t*)"manifest.ttl",
		(const uint8_t*)sord_node_get_string(bundle_node));

	sord_read_file(world->model, manifest_uri.buf, bundle_node,
	               slv2_world_blank_node_prefix(world));

	// ?plugin a lv2:Plugin
	SLV2Matches plug_results = slv2_world_find_statements(
		world, world->model,
		NULL,
		world->rdf_a_node,
		world->lv2_plugin_node,
		bundle_node);
	FOREACH_MATCH(plug_results) {
		SLV2Node plugin_node = slv2_match_subject(plug_results);
		slv2_world_add_plugin(world, plugin_node,
		                      &manifest_uri, NULL, bundle_node);
	}
	slv2_match_end(plug_results);

#ifdef SLV2_DYN_MANIFEST
	typedef void* LV2_Dyn_Manifest_Handle;
	LV2_Dyn_Manifest_Handle handle = NULL;

	// ?dman a dynman:DynManifest
	SLV2Matches dmanifests = slv2_world_find_statements(
		world, world->model,
		NULL,
		world->rdf_a_node,
		world->dyn_manifest_node,
		bundle_node);
	FOREACH_MATCH(dmanifests) {
		SLV2Node dmanifest = slv2_match_subject(dmanifests);

		// ?dman lv2:binary ?binary
		SLV2Matches binaries  = slv2_world_find_statements(
			world, world->model,
			dmanifest,
			world->lv2_binary_node,
			NULL,
			bundle_node);
		if (slv2_matches_end(binaries)) {
			slv2_match_end(binaries);
			SLV2_ERRORF("Dynamic manifest in <%s> has no binaries, ignored\n",
			            slv2_value_as_uri(bundle_uri));
			continue;
		}

		// Get binary path
		SLV2Node       binary   = slv2_node_copy(slv2_match_object(binaries));
		const uint8_t* lib_uri  = sord_node_get_string(binary);
		const char*    lib_path = slv2_uri_to_path((const char*)lib_uri);
		if (!lib_path) {
			SLV2_ERROR("No dynamic manifest library path\n");
			continue;
		}

		// Open library
		void* lib = dlopen(lib_path, RTLD_LAZY);
		if (!lib) {
			SLV2_ERRORF("Failed to open dynamic manifest library `%s'\n", lib_path);
			continue;
		}

		// Open dynamic manifest
		typedef int (*OpenFunc)(LV2_Dyn_Manifest_Handle*, const LV2_Feature *const *);
		OpenFunc open_func = (OpenFunc)slv2_dlfunc(lib, "lv2_dyn_manifest_open");
		if (open_func)
			open_func(&handle, &dman_features);

		// Get subjects (the data that would be in manifest.ttl)
		typedef int (*GetSubjectsFunc)(LV2_Dyn_Manifest_Handle, FILE*);
		GetSubjectsFunc get_subjects_func = (GetSubjectsFunc)slv2_dlfunc(lib,
				"lv2_dyn_manifest_get_subjects");
		if (!get_subjects_func)
			continue;

		// Generate data file
		FILE* fd = tmpfile();
		get_subjects_func(handle, fd);
		rewind(fd);

		// Parse generated data file
		sord_read_file_handle(world->model, fd, lib_uri, bundle_node,
		                      slv2_world_blank_node_prefix(world));

		// Close (and automatically delete) temporary data file
		fclose(fd);

		// ?plugin a lv2:Plugin
		SLV2Matches plug_results = slv2_world_find_statements(
			world, world->model,
			NULL,
			world->rdf_a_node,
			world->lv2_plugin_node,
			bundle_node);
		FOREACH_MATCH(plug_results) {
			SLV2Node plugin_node = slv2_match_subject(plug_results);
			slv2_world_add_plugin(world, plugin_node,
			                      &manifest_uri, binary, bundle_node);
		}
		slv2_match_end(plug_results);
	}
	slv2_match_end(dmanifests);
#endif // SLV2_DYN_MANIFEST

	// ?specification a lv2:Specification
	SLV2Matches spec_results = slv2_world_find_statements(
		world, world->model,
		NULL,
		world->rdf_a_node,
		world->lv2_specification_node,
		bundle_node);
	FOREACH_MATCH(spec_results) {
		SLV2Node spec = slv2_match_subject(spec_results);

		// Add ?specification rdfs:seeAlso <manifest.ttl>
		SordQuad see_also_tup = {
			slv2_node_copy(spec),
			slv2_node_copy(world->rdfs_seealso_node),
			sord_new_uri(world->world, manifest_uri.buf),
			NULL
		};
		sord_add(world->model, see_also_tup);

		// Add ?specification slv2:bundleURI <file://some/path>
		SordQuad bundle_uri_tup = {
			slv2_node_copy(spec),
			slv2_node_copy(world->slv2_bundleuri_node),
			slv2_node_copy(bundle_uri->val.uri_val),
			NULL
		};
		sord_add(world->model, bundle_uri_tup);
	}
	slv2_match_end(spec_results);

	serd_node_free(&manifest_uri);
}

// Expand POSIX things in path (particularly ~)
static char*
expand(const char* path)
{
	char*     ret = NULL;
	wordexp_t p;

	wordexp(path, &p, 0);
	if (p.we_wordc == 0) {
		// Literal directory path (e.g. no variables or ~)
		ret = strdup(path);
	} else if (p.we_wordc == 1) {
		// Directory path expands (e.g. contains ~ or $FOO)
		ret = strdup(p.we_wordv[0]);
	} else {
		// Multiple expansions in a single directory path?
		fprintf(stderr, "lv2config: malformed path `%s' ignored\n", path);
	}

	wordfree(&p);
	return ret;
}

/** Load all bundles in the directory at @a dir_path. */
static void
slv2_world_load_directory(SLV2World world, const char* dir_path)
{
	char* path = expand(dir_path);
	if (!path) {
		return;
	}

	DIR* pdir = opendir(path);
	if (!pdir) {
		free(path);
		return;
	}

	struct dirent* pfile;
	while ((pfile = readdir(pdir))) {
		if (!strcmp(pfile->d_name, ".") || !strcmp(pfile->d_name, ".."))
			continue;

		char* uri = slv2_strjoin("file://",
		                         path, SLV2_DIR_SEP,
		                         pfile->d_name, SLV2_DIR_SEP,
		                         NULL);

		DIR* const bundle_dir = opendir(uri + 7);
		if (bundle_dir) {
			closedir(bundle_dir);
			SLV2Value uri_val = slv2_value_new_uri(world, uri);
			slv2_world_load_bundle(world, uri_val);
			slv2_value_free(uri_val);
		}

		free(uri);
	}

	free(path);
	closedir(pdir);
}

/** Load all bundles found in @a lv2_path.
 * @param lv2_path A colon-delimited list of directories.  These directories
 * should contain LV2 bundle directories (ie the search path is a list of
 * parent directories of bundles, not a list of bundle directories).
 */
static void
slv2_world_load_path(SLV2World   world,
                     const char* lv2_path)
{
	while (lv2_path[0] != '\0') {
		const char* const sep = strchr(lv2_path, SLV2_PATH_SEP[0]);
		if (sep) {
			const size_t dir_len = sep - lv2_path;
			char* const  dir     = malloc(dir_len + 1);
			memcpy(dir, lv2_path, dir_len);
			dir[dir_len] = '\0';
			slv2_world_load_directory(world, dir);
			free(dir);
			lv2_path += dir_len + 1;
		} else {
			slv2_world_load_directory(world, lv2_path);
			lv2_path = "\0";
		}
	}
}

static void
slv2_world_load_specifications(SLV2World world)
{
	SLV2Matches specs = slv2_world_find_statements(
		world, world->model,
		NULL,
		world->rdf_a_node,
		world->lv2_specification_node,
		NULL);
	FOREACH_MATCH(specs) {
		SLV2Node    spec_node = slv2_match_subject(specs);
		SLV2Matches files     = slv2_world_find_statements(
			world, world->model,
			spec_node,
			world->rdfs_seealso_node,
			NULL,
			NULL);
		FOREACH_MATCH(files) {
			SLV2Node file_node = slv2_match_object(files);
			sord_read_file(world->model,
			               (const uint8_t*)sord_node_get_string(file_node),
			               NULL,
			               slv2_world_blank_node_prefix(world));
		}
		slv2_match_end(files);
	}
	slv2_match_end(specs);
}

static void
slv2_world_load_plugin_classes(SLV2World world)
{
	/* FIXME: This loads all classes, not just lv2:Plugin subclasses.
	   However, if the host gets all the classes via slv2_plugin_class_get_children
	   starting with lv2:Plugin as the root (which is e.g. how a host would build
	   a menu), they won't be seen anyway...
	*/

	SLV2Matches classes = slv2_world_find_statements(
		world, world->model,
		NULL,
		world->rdf_a_node,
		world->rdfs_class_node,
		NULL);
	FOREACH_MATCH(classes) {
		SLV2Node class_node = slv2_match_subject(classes);

		// Get parents (superclasses)
		SLV2Matches parents = slv2_world_find_statements(
			world, world->model,
			class_node,
			world->rdfs_subclassof_node,
			NULL,
			NULL);

		if (slv2_matches_end(parents)) {
			slv2_match_end(parents);
			continue;
		}

		SLV2Node parent_node = slv2_node_copy(slv2_match_object(parents));
		slv2_match_end(parents);

		if (!sord_node_get_type(parent_node) == SORD_URI) {
			// Class parent is not a resource, ignore (e.g. owl restriction)
			continue;
		}

		// Get labels
		SLV2Matches labels = slv2_world_find_statements(
			world, world->model,
			class_node,
			world->rdfs_label_node,
			NULL,
			NULL);

		if (slv2_matches_end(labels)) {
			slv2_match_end(labels);
			continue;
		}

		SLV2Node       label_node = slv2_node_copy(slv2_match_object(labels));
		const uint8_t* label      = (const uint8_t*)sord_node_get_string(label_node);
		slv2_match_end(labels);

		SLV2PluginClasses classes = world->plugin_classes;
		SLV2PluginClass   pclass  = slv2_plugin_class_new(
			world, parent_node, class_node, (const char*)label);

		if (pclass) {
			slv2_sequence_insert(classes, pclass);
		}
	}
	slv2_match_end(classes);
}

SLV2_API
void
slv2_world_load_all(SLV2World world)
{
	const char* lv2_path = getenv("LV2_PATH");
	if (!lv2_path)
		lv2_path = SLV2_DEFAULT_LV2_PATH;

	// Discover bundles and read all manifest files into model
	slv2_world_load_path(world, lv2_path);

	SLV2_FOREACH(p, world->plugins) {
		SLV2Plugin plugin     = slv2_collection_get(world->plugins, p);
		SLV2Value  plugin_uri = slv2_plugin_get_uri(plugin);

		// ?new dc:replaces plugin
		SLV2Matches replacement = slv2_world_find_statements(
			world, world->model,
			NULL,
			world->dc_replaces_node,
			slv2_value_as_node(plugin_uri),
			NULL);
		if (!sord_iter_end(replacement)) {
			/* TODO: Check if replacement is actually a known plugin,
			   though this is expensive...
			*/
			plugin->replaced = true;
		}
		slv2_match_end(replacement);
	}

	// Query out things to cache
	slv2_world_load_specifications(world);
	slv2_world_load_plugin_classes(world);
}

SLV2_API
SLV2PluginClass
slv2_world_get_plugin_class(SLV2World world)
{
	return world->lv2_plugin_class;
}

SLV2_API
SLV2PluginClasses
slv2_world_get_plugin_classes(SLV2World world)
{
	return world->plugin_classes;
}

SLV2_API
SLV2Plugins
slv2_world_get_all_plugins(SLV2World world)
{
	return world->plugins;
}

SLV2_API
SLV2Plugin
slv2_world_get_plugin_by_uri_string(SLV2World   world,
                                    const char* uri)
{
	SLV2Value  uri_val = slv2_value_new_uri(world, uri);
	SLV2Plugin plugin  = slv2_plugins_get_by_uri(world->plugins, uri_val);
	slv2_value_free(uri_val);
	return plugin;
}
