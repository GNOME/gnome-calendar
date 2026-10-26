# Accessibility

Accessibility is a core priority for Calendar. In the recent years, we have been putting a lot of (volunteer) effort to make Calendar as accessible as possible for everyone, regardless of individuals' abilities. This includes desired first-class keyboard navigation support, properly supporting and regularly testing with assistive technologies, as well as inquiring in the issue tracker and on social media.

This document explains our accessibility commitment, how contributors can help us uphold it, and how to report accessibility issues. Generally, all of the guidelines for contributors documented in [CONTRIBUTING.md](CONTRIBUTING.md) apply here.

## Reporting Accessibility Issues

There are several ways to report issues related to accessibility:

- [**Create a new topic on GNOME Discourse**](https://discourse.gnome.org/tag/calendar/22) with the “Accessibility” category and the “calendar” tag.
- [**Open an issue on GitLab**](https://gitlab.gnome.org/GNOME/gnome-calendar/-/work_items/new?type=Issue&initialCreationContext=list-route) with the appropriate template. If unsure, use “Bug” and follow the steps in the template.
- [**Join the `#gnome-calendar:gnome.org` Matrix room**](https://matrix.to/#/#gnome-calendar:gnome.org) if you prefer a synchronous communication method.

## Known Limitations

The entirety of Calendar's interface is tested manually, not by automation. This means that some regressions might appear in future versions without anyone's knowledge if a particular behavior is left untested. As such, it is desired to test as much as possible before making releases.

## Feedback

Accessibility is an ongoing practice, not a one-time fix. If you have suggestions for improving this statement or our practices, please [create a topic](https://discourse.gnome.org/tag/calendar/22) in GNOME Discourse, [open an issue](https://gitlab.gnome.org/GNOME/gnome-calendar/-/work_items/new?type=Issue&initialCreationContext=list-route) on GitLab, or [message us](https://matrix.to/#/#gnome-calendar:gnome.org) on Matrix.

## For Developers & Researchers: Useful Resources

- [Accessibility](https://developer.gnome.org/hig/guidelines/accessibility.html) guidelines in the GNOME Human Interface Guidelines
- [`GtkAccessible`](https://docs.gtk.org/gtk4/iface.Accessible.html)
- [`GtkAccessibleHypertext`](https://docs.gtk.org/gtk4/iface.AccessibleHypertext.html)
- [`GtkAccessibleRange`](https://docs.gtk.org/gtk4/iface.AccessibleRange.html)
- [`GtkAccessibleText`](https://docs.gtk.org/gtk4/iface.AccessibleHypertext.html)
- [GTK4 Accessibility page](https://docs.gtk.org/gtk4/section-accessibility.html)
- [Date Picker Dialog Example](https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/examples/datepicker-dialog/) and [Date Picker Combobox Example](https://www.w3.org/WAI/ARIA/apg/patterns/combobox/examples/combobox-datepicker/) from ARIA Authoring Practices Guide (APG)

## For Testers: What & How to Test

This is a detailed list of custom widgets to test their behavior in relation to keyboard navigation.

- In the month or year chooser spin buttons, arrow up must go to the next date, and arrow down must go to the previous date. On mobile form factor, the date chooser spin button must additionally expand when pressing Enter/Space, and show the month and year choosers.
- In the calendar grid, Tab or Shift+Tab must leave focus.
- In the month view:
  - When focused on an event:
    - Pressing Tab must focus the next event.
    - Shift+Tab must focus the previous event.
    - Pressing arrow keys must focus the event that is adjacent based on the direction.
    - When there are no events left, arrow keys must emit a sound.
    - When there are no events left, Tab must focus the month cell behind the event.
    - Control+Tab must focus the month cell behind the focused event.
    - Activating an event must display the event details popover.
    - When an overflow button (“+1”) is present, the button must follow the same procedure as events.
  - When focused on a month cell:
    - Arrow keys must focus the adjacent cell.
    - At the beginning of the row, pressing the left arrow key on left-to-right direction or right arrow key on right-to-left must focus the last cell on the row above.
    - At the end of the row, pressing the right arrow key on left-to-right direction or left arrow key on right-to-left must focus the first cell on the row below.
    - Pressing Tab must leave focus.
    - Pressing Shift+Tab must focus the first event that is right in front of the focused cell; if there are no events in front, then it must leave focus.
    - On the first row, pressing arrow up must scroll up.
    - On the last row, pressing arrow down must scroll down.
    - Holding down Control and pressing an arrow key must go to the end of the direction.
    - Holding down Shift while navigating with the keyboard must select every cell between the first and last cells.

<!-- TODO: screen reader testing -->
