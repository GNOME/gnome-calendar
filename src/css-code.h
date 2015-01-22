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

#define CSS_TEMPLATE "GcalEventWidget.color-%d {"\
                     "  background-color: %s;"\
                     "}"

#define CSS_TEMPLATE_EDGE_BOTH "GcalEventWidget.color-%d.slanted:dir(ltr) {"\
                               "  background-color: transparent;"\
                               "  padding: 0 2em;"\
                               "  border-radius: 0;"\
                               "  background-image: linear-gradient(-80deg, %s,"\
                               "                                    %s 40%%,"\
                               "                                    alpha(%s, 0) 45%%,"\
                               "                                    alpha(%s, 0) 60%%,"\
                               "                                    %s 65%%,"\
                               "                                    %s 75%%,"\
                               "                                    alpha(%s, 0) 80%%),"\
                               "                    linear-gradient(100deg, %s,"\
                               "                                    %s 40%%,"\
                               "                                    alpha(%s, 0) 45%%,"\
                               "                                    alpha(%s, 0) 60%%,"\
                               "                                    %s 65%%,"\
                               "                                    %s 75%%,"\
                               "                                    alpha(%s, 0) 80%%),"\
                               "                    linear-gradient(to top, %s);"\
                               "  background-size: 2em, 2em, 100%% 100%%;"\
                               "  background-repeat: no-repeat;"\
                               "  background-clip: border-box, border-box, content-box;"\
                               "  background-position: top left, top right;"\
                               "}"

#define CSS_TEMPLATE_EDGE_BOTH_RTL "GcalEventWidget.color-%d.slanted:dir(rtl) {"\
                                   "  background-color: transparent;"\
                                   "  padding: 0 2em;"\
                                   "  border-radius: 0;"\
                                   "  background-image: linear-gradient(-100deg, %s,"\
                                   "                                    %s 40%%,"\
                                   "                                    alpha(%s, 0) 45%%,"\
                                   "                                    alpha(%s, 0) 60%%,"\
                                   "                                    %s 65%%,"\
                                   "                                    %s 75%%,"\
                                   "                                    alpha(%s, 0) 80%%),"\
                                   "                    linear-gradient(80deg, %s,"\
                                   "                                    %s 40%%,"\
                                   "                                    alpha(%s, 0) 45%%,"\
                                   "                                    alpha(%s, 0) 60%%,"\
                                   "                                    %s 65%%,"\
                                   "                                    %s 75%%,"\
                                   "                                    alpha(%s, 0) 80%%),"\
                                   "                    linear-gradient(to top, %s);"\
                                   "  background-size: 2em, 2em, 100%% 100%%;"\
                                   "  background-repeat: no-repeat;"\
                                   "  background-clip: border-box, border-box, content-box;"\
                                   "  background-position: top left, top right;"\
                                   "}"

#define CSS_TEMPLATE_EDGE "GcalEventWidget.color-%d.slanted-end:dir(ltr) {"\
                          "  background-color: transparent;"\
                          "  padding-right: 2em;"\
                          "  border-top-right-radius: 0;"\
                          "  border-bottom-right-radius: 0;"\
                          "  background-image: linear-gradient(100deg, %s,"\
                          "                                    %s 40%%,"\
                          "                                    alpha(%s, 0) 45%%,"\
                          "                                    alpha(%s, 0) 60%%,"\
                          "                                    %s 65%%,"\
                          "                                    %s 75%%,"\
                          "                                    alpha(%s, 0) 80%%),"\
                          "                    linear-gradient(to top, %s);"\
                          "  background-size: 2em, 100%% 100%%;"\
                          "  background-repeat: no-repeat;"\
                          "  background-position: top right, -2em;"\
                          "}"

#define CSS_TEMPLATE_EDGE_START "GcalEventWidget.color-%d.slanted-start:dir(ltr) {"\
                                "  background-color: transparent;"\
                                "  padding-left: 2em;"\
                                "  border-top-left-radius: 0;"\
                                "  border-bottom-left-radius: 0;"\
                                "  background-image: linear-gradient(-80deg, %s,"\
                                "                                    %s 40%%,"\
                                "                                    alpha(%s, 0) 45%%,"\
                                "                                    alpha(%s, 0) 60%%,"\
                                "                                    %s 65%%,"\
                                "                                    %s 75%%,"\
                                "                                    alpha(%s, 0) 80%%),"\
                                "                    linear-gradient(to top, %s);"\
                                "  background-size: 2em, 100%% 100%%;"\
                                "  background-repeat: no-repeat;"\
                                "  background-position: top left, 2em;"\
                                "}"

#define CSS_TEMPLATE_EDGE_RTL "GcalEventWidget.color-%d.slanted-end:dir(rtl) {"\
                              "  background-color: transparent;"\
                              "  padding-left: 2em;"\
                              "  border-top-left-radius: 0;"\
                              "  border-bottom-left-radius: 0;"\
                              "  background-image: linear-gradient(-100deg, %s,"\
                              "                                    %s 40%%,"\
                              "                                    alpha(%s, 0) 45%%,"\
                              "                                    alpha(%s, 0) 60%%,"\
                              "                                    %s 65%%,"\
                              "                                    %s 75%%,"\
                              "                                    alpha(%s, 0) 80%%),"\
                              "                    linear-gradient(to top, %s);"\
                              "  background-size: 2em, 100%% 100%%;"\
                              "  background-repeat: no-repeat;"\
                              "  background-position: top left, 2em;"\
                              "}"

#define CSS_TEMPLATE_EDGE_START_RTL "GcalEventWidget.color-%d.slanted-start:dir(rtl) {"\
                                    "  background-color: transparent;"\
                                    "  padding-right: 2em;"\
                                    "  border-top-right-radius: 0;"\
                                    "  border-bottom-right-radius: 0;"\
                                    "  background-image: linear-gradient(80deg, %s,"\
                                    "                                    %s 40%%,"\
                                    "                                    alpha(%s, 0) 45%%,"\
                                    "                                    alpha(%s, 0) 60%%,"\
                                    "                                    %s 65%%,"\
                                    "                                    %s 75%%,"\
                                    "                                    alpha(%s, 0) 80%%),"\
                                    "                    linear-gradient(to top, %s);"\
                                    "  background-size: 2em, 100%% 100%%;"\
                                    "  background-repeat: no-repeat;"\
                                    "  background-position: top right, -2em;"\
                                    "}"



#endif /* CSS_CODE_H */

