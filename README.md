# Islandora Book Batch [![Build Status](https://travis-ci.org/Islandora/islandora_batch.png?branch=7.x)](https://travis-ci.org/Islandora/islandora_batch)

## Introduction

This module implements a batch framework for importing books into Islandora.

The ingest is a two-step process:

* Preprocessing: The data is scanned and a number of entries are created in the
  Drupal database.  There is minimal processing done at this point, so preprocessing can
  be completed outside of a batch process.
* Ingest: The data is actually processed and ingested. This happens inside of
  a Drupal batch.

## Requirements

This module requires the following modules/libraries:

* [Islandora](https://github.com/islandora/islandora)
* [Tuque](https://github.com/islandora/tuque)
* [Islandora Batch](https://github.com/Islandora/islandora_batch)
* [Book Solution Pack](https://github.com/Islandora/islandora_solution_pack_book)


# Installation

Install as usual, see [this](https://drupal.org/documentation/install/modules-themes/modules-7) for further information.

## Configuration

N/A

## Documentation

Further documentation for this module is available at [our wiki](https://wiki.duraspace.org/display/ISLANDORA/How+to+Batch+Ingest+Files).

### Usage

The base ZIP/directory preprocessor can be called as a drush script (see `drush help islandora_book_batch_preprocess` for additional parameters):

`drush -v --user=admin --uri=http://localhost islandora_book_batch_preprocess --type=zip --scan_target=/path/to/archive.zip`

This will populate the queue (stored in the Drupal database) with base entries.

Books must be broken up into separate directories, such that each directory at the "top" level (in the scan_target directory or Zip file) represents a book. Book pages are their own directories inside of each book directory.

Files are assigned to object datastreams based on their basename, so a folder structure like:

* my_cool_book/
    * MODS.xml
    * 1/
        * OBJ.tiff
    * 2/
        * OBJ.tiff

would result in a two-page book.

Each page directory name will be used as the sequence number of the page created.

A file named --METADATA--.xml can contain either MODS, DC or MARCXML which is used to fill in the MODS or DC streams (if not provided explicitly). Similarly, --METADATA--.mrc (containing binary MARC) will be transformed to MODS and then possibly to DC, if neither are provided explicitly.

If no MODS is provided at the book level - either directly as MODS.xml, or transformed from either a DC.xml or the "--METADATA--" file discussed above - the directory name will be used as the title.

The queue of preprocessed items can then be processed:

`drush -v --user=admin --uri=http://localhost islandora_batch_ingest`

### Customization

Custom ingests can be written by [extending](https://github.com/Islandora/islandora_batch/wiki/How-To-Extend) any of the existing preprocessors and batch object implementations.

## Troubleshooting/Issues

Having problems or solved a problem? Check out the Islandora google groups for a solution.

* [Islandora Group](https://groups.google.com/forum/?hl=en&fromgroups#!forum/islandora)
* [Islandora Dev Group](https://groups.google.com/forum/?hl=en&fromgroups#!forum/islandora-dev)

## Maintainers/Sponsors

Current maintainers:

* [Jared Whiklo](https://github.com/whikloj)

## Development

If you would like to contribute to this module, please check out [CONTRIBUTING.md](CONTRIBUTING.md). In addition, we have helpful [Documentation for Developers](https://github.com/Islandora/islandora/wiki#wiki-documentation-for-developers) info, as well as our [Developers](http://islandora.ca/developers) section on the [Islandora.ca](http://islandora.ca) site.

## License

[GPLv3](http://www.gnu.org/licenses/gpl-3.0.txt)
