Islandora Book Batch Ingest

CONTENTS OF THIS FILE
---------------------

 * summary
 * requirements
 * installation
 * configuration
 * usage
 * customization
 * troubleshooting
 * faq
 * contact
 * sponsors


SUMMARY
-------

This module plugs into the islandora_batch batch framework, to allow full books
to be ingested.

The ingest is a two-step process:
* Preprocessing: The data is scanned, and a number of entries created in the
  Drupal database.  There is minimal processing done at this point, so it can
  complete outside of a batch process.
* Ingest: The data is actually processed and ingested. This happens inside of
  a Drupal batch.

REQUIREMENTS
------------

Both the islandora_batch and the islandora_book modules must be installed.

INSTALLATION
------------

Install in the same way as any other Drupal module.

CONFIGURATION
-------------

USAGE
-------------

We extend the base ZIP/directory preprocessor, so we can be called something
like (see "drush help islandora_book_batch_preprocess" for
additional parameters):
"drush -v --user=admin --uri=http://localhost islandora_book_batch_preprocess --type=zip --target=/path/to/archive.zip"
should serve to populate the queue (stored in the Drupal database) with base entries.

Books must be broken up into separate directories, such that each directory at
the "top" level (in the target directory or Zip file) represents a book. Book
pages are their own directories inside of each book directory.
Files are assigned to object datastreams based on their basename, so a folder
structure like:
* my_cool_book/
    * MODS.xml
    * 1/
        * OBJ.tiff
    * 2/
        * OBJ.tiff
would result in a two-page book.

A file named --METADATA--.xml can contain either MODS, DC or MARCXML which we
will use to fill in the MODS or DC streams (if not provided explicitly).
Similarly, --METADATA--.mrc (containing binary MARC) will be transformed to
MODS and then possibly to DC, if neither are provided explicitly.

If no MODS is provided at the book-level, the directory name will be used as
the title.

The ingest of preprocessed items remains the same--something like:
"drush -v --user=admin --uri=http://localhost islandora_batch_ingest"

CUSTOMIZATION
-------------

TROUBLESHOOTING
---------------

F.A.Q.
------


CONTACT
-------


SPONSORS
--------

