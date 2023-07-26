# CRADLE documentation

CRADLE is a framework that intends to speed up lengthy calculations.
A calculation is modeled as a tree of subcalculations. Results of these subcalculations
are cached, so a following calculation that is similar to the previous one can use
these cached subresults, instead of calculating them again.

Read more [here](abstract.md).

## Contents
User documentation
* [Abstract](abstract.md) Read this first
  * [Contexts](context.md)
* [Example: make some blob](make_some_blob.md)
* [Configuration file](config_template.toml)
* [Remote (cloud-based) cache](remote_cache.md)

Design documentation
* [Debugging](debugging.md)
* [Lambda captures](lambda_captures.md)
* [Capturing cache keys](capturing_cache_keys.md)
* [Resolving a serialized request](seri_resolver.md)

Updating documentation
* [Updating documentation](update_docs.md)

CRADLE was originally created to act as a local proxy for Thinknode.
Thinknode-specific documentation:
* [Introduction](thinknode_intro.md)
* [Immutable data](thinknode_data.md)
* [Cache](thinknode_cache.md)
* [Messages](thinknode_msg_overview.md)
