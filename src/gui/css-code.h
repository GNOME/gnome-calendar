/* css-code.h
 *
 * Copyright (C) 2015 Erick Pérez Castellanos <erick.red@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CSS_CODE_H
#define CSS_CODE_H

#define CSS_TEMPLATE \
".color-%1$d {"\
"  background-color: %2$s"\
"}"\
".color-%1$d.horizontal.timed {"\
"  background-color: transparent;"\
"}"\
".color-%1$d.horizontal.timed box.icon-box{"\
"  background-color: %2$s;"\
"  border-radius: 2px;"\
"  min-height: 16px;"\
"  min-width: 16px;"\
"}"\
".color-%1$d.slanted {"\
"  background-color: transparent;"\
"  background-image: linear-gradient(100deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(280deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(100deg, /* left edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px),"\
"                    linear-gradient(280deg, /* right edge shadow*/"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"  border-bottom: 1px solid transparent; /* for the shadow displacement */"\
"  background-size: 50%% 100%%, 50%% 100%%, 50%% 100%%, 50%% 100%%;"\
"  background-position: left bottom, right bottom, left bottom, right bottom;"\
"  background-repeat: no-repeat;"\
"  background-origin: padding-box, padding-box, border-box, border-box;"\
"  background-clip: padding-box, padding-box, border-box, border-box;"\
"  padding-left: 24px;"\
"  padding-right: 24px;"\
"  padding-top: 3px;"\
"  padding-bottom: 4px;"\
"  border-radius: 0;"\
"}"\
".color-%1$d.slanted:backdrop {"\
"  background-image: linear-gradient(100deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(280deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted:dir(rtl) {"\
"  background-image: linear-gradient(80deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(260deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(80deg, /* left edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px),"\
"                    linear-gradient(260deg, /* right edge shadow*/"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"}"\
".color-%1$d.slanted:dir(rtl):backdrop {"\
"  background-image: linear-gradient(80deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(260deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted-start {"\
"  background-color: transparent;"\
"  background-image: linear-gradient(100deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(100deg, /* left edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"  border-bottom: 1px solid transparent; /* for the shadow displacement */"\
"  background-position: left bottom, left bottom;"\
"  background-size: 100%% 100%%, 100%% 100%%;"\
"  background-origin: padding-box, border-box;"\
"  background-clip: padding-box, border-box;"\
"  border-radius: 0 2px 2px 0;"\
"  padding-right: 4px;"\
"  padding-left: 24px;"\
"  padding-top: 3px;"\
"  padding-bottom: 4px;"\
"}"\
".color-%1$d.slanted-start:backdrop {"\
"  background-image: linear-gradient(100deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted-start:dir(rtl) {"\
"  background-image: linear-gradient(260deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(260deg, /* right edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"  border-bottom: 1px solid transparent; /* for the shadow displacement */"\
"  background-position: right bottom, right bottom;"\
"  border-radius: 2px 0 0 2px;"\
"  padding-right: 24px;"\
"  padding-left: 4px;"\
"}"\
".color-%1$d.slanted-start:dir(rtl):backdrop {"\
"  background-image: linear-gradient(260deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted-end {"\
"  background-color: transparent;"\
"  background-image: linear-gradient(280deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(280deg, /* right edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"  border-bottom: 1px solid transparent; /* for the shadow displacement */"\
"  background-position: right bottom, right bottom;"\
"  background-size: 100%% 100%%, 100%% 100%%;"\
"  background-origin: padding-box, border-box;"\
"  background-clip: padding-box, border-box;"\
"  border-radius: 2px 0 0 2px;"\
"  padding-right: 24px;"\
"  padding-left: 4px;"\
"  padding-top: 3px;"\
"  padding-bottom: 4px;"\
"}"\
".color-%1$d.slanted-end:backdrop {"\
"  background-image: linear-gradient(280deg, /* right edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted-end:dir(rtl) {"\
"  background-image: linear-gradient(80deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px),"\
"                    linear-gradient(80deg, /* left edge shadow */"\
"                                    transparent          5px,"\
"                                    @event_shadow_color  6px,"\
"                                    @event_shadow_color  9px,"\
"                                    transparent         10px,"\
"                                    transparent         16px,"\
"                                    @event_shadow_color	17px);"\
"  background-position: left bottom, left bottom;"\
"  border-radius: 0 2px 2px 0;"\
"  padding-right: 4px;"\
"  padding-left: 24px;"\
"}"\
".color-%1$d.slanted-end:dir(rtl):backdrop {"\
"  background-image: linear-gradient(80deg, /* left edge */"\
"                                    alpha(%2$s, 0)  5px,"\
"                                    %2$s            6px,"\
"                                    %2$s	    9px,"\
"                                    alpha(%2$s, 0) 10px,"\
"                                    alpha(%2$s, 0) 16px,"\
"                                    %2$s	   17px);"\
"}"\
".color-%1$d.slanted box { margin: 0 16px 0 16px; }"\
".color-%1$d.slanted-start box { margin-left: 16px; }"\
".color-%1$d.slanted-start:dir(rtl) box { margin-right: 16px; }"\
".color-%1$d.slanted-end box { margin-right: 16px; }"\
".color-%1$d.slanted-end:dir(rtl) box { margin-left: 16px; }"

#endif /* CSS_CODE_H */

