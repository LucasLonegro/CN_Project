#include "graph_drawing.h"
#include <stdlib.h>

void print_graph(const network_t *network, label_generator labeler, void *data, const coordinate *node_coordinates, path_drawing *paths, uint64_t paths_size, double x_scale, double y_scale, FILE *file, print_graph_options options)
{
    fprintf(file, "\\begin{tikzpicture}[node distance=2cm,>=stealth',auto, every place/.style={draw}, baseline]\n");
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        fprintf(file, "\\coordinate(C%ld) at (%.3f,%.3f);\n", i + 1, node_coordinates[i].x * x_scale, node_coordinates[i].y * y_scale);
        fprintf(file, "\\coordinate(B%ld) at (%.3f,%.3f);\n", i + 1, node_coordinates[i].x * x_scale, (node_coordinates[i].y - 0.5) * y_scale);
        fprintf(file, "\\coordinate(A%ld) at (%.3f,%.3f);\n", i + 1, node_coordinates[i].x * x_scale, (node_coordinates[i].y + 0.5) * y_scale);
        fprintf(file, "\\node [place] (S%ld) at (%.3f,%.3f) {%ld};\n", i + 1, node_coordinates[i].x * x_scale, node_coordinates[i].y * y_scale, i + 1);
    }
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if ((i >= j && !(options & REVERSE_ARROWS)) || (i <= j && (options & REVERSE_ARROWS)))
                continue;
            __ssize_t link_weight = get_link_weight(network, i, j);
            if (link_weight != -1)
            {
                fprintf(file, "\\path[%s] (S%ld) edge node {%s} (S%ld);\n", options & DRAW_ARROWS ? "->" : "", i + 1, labeler(data, i, j), j + 1);
            }
        }
    }
    for (uint64_t i = 0; i < paths_size; i++)
    {
        for (uint64_t j = 0; j < paths[i].path->length; j++)
        {
            fprintf(file, "\\definecolor{color%ld}{RGB}{%d,%d,%d}", i * network->node_count + j, paths[i].color.red, paths[i].color.green, paths[i].color.blue);
            fprintf(file, "\\path[->%s,%s%ld, line width = 0.5mm] (B%ld) edge node {%s} (B%ld);\n", paths[i].line == SOLID_LINE ? "" : paths[i].line == DOTTED_LINE ? ",dotted"
                                                                                                                                                                    : ",dashed",
                    "color", i * network->node_count + j,
                    paths[i].path->nodes[j] + 1, paths[i].labeler == NULL ? "" : paths[i].labeler(paths[i].data, paths[i].path->nodes[j], paths[i].path->nodes[j + 1]), paths[i].path->nodes[j + 1] + 1);
        }
    }
    fprintf(file, " \\end{tikzpicture}\n");
}

void print_linebreak(FILE *file)
{
    fprintf(file, "\\bigskip\\\\\n");
}

void print_plot(bar_plot plot, FILE *file)
{
    fprintf(file,
            "\n\\begin{tikzpicture}\
\n\\begin{axis}[\
\n	x tick label style={\
\n      rotate=90,\
\n		/pgf/number format/1000 sep=},\
\n	ylabel=%s,\
\n	enlargelimits=0.05,\
\n	legend style={at={(0.5,-0.2)},\
\n	anchor=north,legend columns=-1},\
\n	ybar=0.7,\
\n  xminorgrids=true,\
\n  minor tick num=1,\
\n  major tick style={draw=none},\
\n  enlarge x limits=0.125,\
\n  xtick=data,\
\n  width=%.3fcm,\
\n  bar width=%.3fmm,\
\n  symbolic x coords={",
            plot.y_label, plot.width, plot.width / plot.data_points_count * 10 - 5);
    for (uint64_t i = 0; i < plot.x_labels_count; i++)
    {
        fprintf(file, "%s", plot.x_labels[i]);
        if (i != plot.x_labels_count - 1)
        {
            fprintf(file, ",");
        }
    }
    fprintf(file, "},\n]");
    for (uint64_t i = 0; i < plot.samples_count; i++)
    {
        fprintf(file,
                "\n\\addplot \
\n	coordinates {");
        for (uint64_t j = 0; j < plot.data_points_count; j++)
        {
            fprintf(file, "(%s,%.3f) ", plot.data_points[i * plot.data_points_count + j].x, plot.data_points[i * plot.data_points_count + j].y);
        }
        fprintf(file, "};");
    }

    fprintf(file,
            "\n\\legend{");
    for (uint64_t i = 0; i < plot.samples_count; i++)
    {
        fprintf(file, "%s", plot.legends[i]);
        if (i != plot.samples_count - 1)
            fprintf(file, ",");
    }
    fprintf(file,
            "}");

    fprintf(file,
            "\n\\end{axis}\
\n\\end{tikzpicture}");
}

void print_section(FILE *file, char *section)
{
    fprintf(file, "\\pagebreak\n\\section*{%s}", section);
}

void print_prologue(FILE *file, char *title)
{
    fprintf(file, "\
\\documentclass[10pt]{article}\
\n\\usepackage{mathtools}\
\n\\usepackage{amssymb}\
\n\\usepackage{enumitem}\
\n\\usepackage[left=1.5cm, right=2cm, top=1cm, bottom=2cm]{geometry}\
\n\\usepackage{tikz}\
\n\\usepackage{pgfplots}\
\n\\pgfplotsset{compat=1.18}\
\n\\usetikzlibrary{arrows,shapes,automata,petri,positioning,calc}\
\n\\setlength\\parindent{0pt}\
\n\\title{%s}\
\n\\date{}\
\n\\author{}\
\n\\newcommand*\\circled[1]{\\tikz[baseline=(char.base)]{\
\n            \\node[shape=circle,draw,inner sep=2pt] (char) {#1};}}\
\n\\newcommand{\\caselist}[1]{\\begin{enumerate}[label=\\protect\\circled{C\\arabic*}]#1\\end{enumerate}}\
\n\\newcommand*{\\true}{\\circled{V}}\
\n\\newcommand*{\\false}{\\circled{F}}\
\n\\newcommand\\xeq[1]{\\,\\stackrel{\\mathclap{\\normalfont\\mbox{\\tiny{#1}}}}{=}\\,}\
\n\
\n\\begin{document}\
\n\\maketitle\
\n",
            title);
}
void print_epilogue(FILE *file)
{
    fprintf(file, "\n\\end{document}");
}
