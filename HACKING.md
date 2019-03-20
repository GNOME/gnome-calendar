# Style

GNOME Calendar has a coding style based on GTK Coding Style, but with a few more
rules. Please read them carefully and, if in doubt, ask a maintainer for directions.

## General

The most important rule is: **see the surrounding code, and copy its style**.

Another rule that applies to function declarations is that all parameters are
aligned by the last '*'. There are plenty of examples below.

## Header (.h) files

 * The '*' and the type come together, without any spaces in between.
 * Function name starts at column 22.
 * Parenthesis after function name is at column 66
 * The last '*' in parameters are at column 86

As an example, this is how a function should look like:

```c
GcalManager*         gcal_manager_foo_bar                        (GcalManager        *self,
                                                                  GError            **example);
```

## Source code

The source file keeps an order of methods. The order will be as following:

  1. GPL header
  2. Structures
  3. Function prototypes
  4. G_DEFINE_TYPE()
  5. Enums
  6. Static variables
  7. Auxiliary methods
  8. Signal callbacks
  9. Interface implementations
  10. Parent class overrides
  11. class_init and init
  12. Public API

### Structures

The structures must have the last pointer asterisk at column 22. Non-pointer fields
start at column 23. For example:

```c
struct _GcalQuickAddPopover
{
  GtkPopover          parent;

  GtkWidget          *add_button;
  GtkWidget          *calendars_listbox;
  GtkWidget          *calendar_name_label;
  GtkWidget          *color_image;
  GtkWidget          *stack;
  GtkWidget          *summary_entry;
  GtkWidget          *title_label;

  /* Internal data */
  GDateTime          *date_start;
  GDateTime          *date_end;

  GtkWidget          *selected_row;

  gboolean            clock_format_24h;

  GcalManager        *manager;
};
```

### Function Prototypes

Function prototypes must be formatted just like in header files.

### Auxiliary Methods

Auxiliary method names must have a verb in the dictionary form, and should always
perform an action over something. They don't have the gcal_foo prefix. For example:

```c
static void
do_something_on_data (Foo *data,
                      Bar *bar)
{
  /* ... */
}
```

### Signal Callbacks

Signal callback names must have the name of the signal in the past. They don't have
the gcal_foo prefix as well, but have a _cb suffix. For example:

```c
static void
size_allocated_cb (GtkWidget     *widget,
                   GtkAllocation *allocation,
                   gpointer       user_data)
{
  /* ... */
}
```

### Line Splitting

Line splitting works following the GTK code style, but legibility comes over above
all. If a function call looks unbalanced following the GTK style, it is fine to
slightly escape the rules.

For example, this feels extremelly unbalanced:

```c
foo_bar_do_somthing_sync (a,
                          1,
                          object,
                          data,
                          something
                          cancellable,
                          &error);
```

Notice the empty space before the arguments, and how empty and odd it looks. In
comparison, it will look better if written like this:

```c
foo_bar_do_somthing_sync (a, 1, object, data,
                          something
                          cancellable,
                          &error);
```

# Contributing guidelines

See CONTRIBUTING.md file for the contribution guidelines, and the Code of Conduct
that contributors are expected to follow.
