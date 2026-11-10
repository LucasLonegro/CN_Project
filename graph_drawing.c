#include "graph_drawing.h"
#include <stdlib.h>
#include <inttypes.h>

#define PATH_OFFSET_MM 1.5
#define SPECTRUM_ROW_HEIGHT 0.32

static const hex_color bar_colors[] = {{66, 133, 244}, {234, 67, 53}, {251, 188, 5}, {52, 168, 83}};
static const hex_color scale_low = {46, 204, 113}, scale_mid = {241, 196, 15}, scale_high = {231, 76, 60};

static void print_color(FILE *file, hex_color c)
{
    fprintf(file, "{rgb,255:red,%d;green,%d;blue,%d}", c.red, c.green, c.blue);
}

static void begin_fit(FILE *file)
{
    fprintf(file, "\\begin{center}\\begin{adjustbox}{max width=\\linewidth}\n");
}

static void end_fit(FILE *file)
{
    fprintf(file, "\\end{adjustbox}\\end{center}\n");
}

void print_graph(const network_t *network, edge_style edges, const coordinate *node_coordinates, path_drawing *paths, uint64_t paths_size, double x_scale, double y_scale, FILE *file, print_graph_options options)
{
    if (options & FIT_TO_PAGE)
        begin_fit(file);
    fprintf(file, "\\begin{tikzpicture}[baseline, >={Stealth[length=2mm]},\n"
                  "  netnode/.style={circle, draw, thick, fill=gray!10, minimum size=7mm, inner sep=1pt},\n"
                  "  link/.style={%s},\n"
                  "  linklabel/.style={font=\\scriptsize, inner sep=1pt, fill=white, fill opacity=0.85, text opacity=1}]\n",
            options & THICK_EDGES ? "line width=1.4pt" : "thick");
    for (uint64_t i = 0; i < network->node_count; i++)
        fprintf(file, "\\node[netnode] (S%" PRIu64 ") at (%.3f,%.3f) {%" PRIu64 "};\n", i + 1, node_coordinates[i].x * x_scale, node_coordinates[i].y * y_scale, i + 1);

    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if (i == j || get_link_weight(network, i, j) == -1)
                continue;
            if (!(options & BIDIRECTIONAL) && ((options & REVERSE_ARROWS) ? i < j : i > j))
                continue;
            fprintf(file, "\\draw[link%s", options & (DRAW_ARROWS | BIDIRECTIONAL) ? ", ->" : "");
            if (edges.colorer != NULL)
            {
                fprintf(file, ", draw=");
                print_color(file, edges.colorer(edges.color_data, i, j));
            }
            fprintf(file, "] (S%" PRIu64 ") to%s", i + 1, options & BIDIRECTIONAL ? "[bend left=12]" : "");
            if (edges.labeler != NULL)
                fprintf(file, " node[linklabel%s] {%s}", options & BIDIRECTIONAL ? ", auto" : "", edges.labeler(edges.label_data, i, j));
            fprintf(file, " (S%" PRIu64 ");\n", j + 1);
        }
    }

    for (uint64_t i = 0; i < paths_size; i++)
    {
        const path_t *path = paths[i].path;
        if (path == NULL || path->length <= 0)
            continue;
        double shift = paths[i].offset == OFFSET_UP ? PATH_OFFSET_MM : paths[i].offset == OFFSET_DOWN ? -PATH_OFFSET_MM : 0;
        for (__ssize_t j = 0; j < path->length; j++)
        {
            fprintf(file, "\\draw[->, line width=0.5mm%s, transform canvas={yshift=%.1fmm}, draw=", paths[i].line == SOLID_LINE ? "" : paths[i].line == DOTTED_LINE ? ", dotted" : ", dashed", shift);
            print_color(file, paths[i].color);
            fprintf(file, "] (S%" PRIu64 ") to node[linklabel, auto] {%s} (S%" PRIu64 ");\n",
                    path->nodes[j] + 1, paths[i].labeler == NULL ? "" : paths[i].labeler(paths[i].data, path->nodes[j], path->nodes[j + 1]), path->nodes[j + 1] + 1);
        }
    }
    fprintf(file, "\\end{tikzpicture}\n");
    if (options & FIT_TO_PAGE)
        end_fit(file);
}

void print_plot(bar_plot plot, FILE *file)
{
    uint64_t points = plot.data_points_count > 0 ? plot.data_points_count : 1;
    uint64_t samples = plot.samples_count > 0 ? plot.samples_count : 1;
    double bar_width = 0.6 * plot.width / ((points + 1) * samples);
    if (bar_width > 1.0)
        bar_width = 1.0;

    fprintf(file, "\n\\begin{center}\\begin{tikzpicture}\n\\begin{axis}[\n"
                  "  ybar, bar width=%.3fcm, width=%.3fcm, height=7cm,\n"
                  "  ymin=0, ymajorgrids, grid style={gray!25}, axis lines*=left,\n"
                  "  enlarge x limits={abs=%.3fcm}, xtick=data, major x tick style={draw=none},\n"
                  "  x tick label style={rotate=90, font=\\scriptsize, /pgf/number format/1000 sep=},\n"
                  "  legend style={at={(0.5,-0.3)}, anchor=north, legend columns=-1, draw=none},\n"
                  "  ylabel={%s},\n",
            bar_width, plot.width, bar_width * samples, plot.y_label);
    if (plot.x_label != NULL)
        fprintf(file, "  xlabel={%s},\n", plot.x_label);
    fprintf(file, "  symbolic x coords={");
    for (uint64_t i = 0; i < plot.x_labels_count; i++)
        fprintf(file, "%s%s", plot.x_labels[i], i != plot.x_labels_count - 1 ? "," : "");
    fprintf(file, "}]\n");

    for (uint64_t i = 0; i < plot.samples_count; i++)
    {
        hex_color c = bar_colors[i % (sizeof(bar_colors) / sizeof(bar_colors[0]))];
        fprintf(file, "\\addplot[fill=");
        print_color(file, c);
        fprintf(file, ", draw=none] coordinates {");
        for (uint64_t j = 0; j < plot.data_points_count; j++)
        {
            data_point p = plot.data_points[i * plot.data_points_count + j];
            fprintf(file, "(%s,%.3f) ", p.x, p.y);
        }
        fprintf(file, "};\n");
    }

    if (plot.legends != NULL)
    {
        fprintf(file, "\\legend{");
        for (uint64_t i = 0; i < plot.samples_count; i++)
            fprintf(file, "%s%s", plot.legends[i], i != plot.samples_count - 1 ? "," : "");
        fprintf(file, "}\n");
    }
    fprintf(file, "\\end{axis}\n\\end{tikzpicture}\\end{center}\n");
}

void print_spectrum_map(spectrum_map map, FILE *file)
{
    uint64_t step = map.slot_count <= 100 ? 10 : map.slot_count <= 200 ? 20 : 40;
    map.slot_count = (map.slot_count + step - 1) / step * step;
    begin_fit(file);
    fprintf(file, "\\begin{tikzpicture}[x=%.4fcm, y=-%.2fcm]\n", map.width / map.slot_count, SPECTRUM_ROW_HEIGHT);
    for (uint64_t r = 0; r < map.row_count; r++)
    {
        const dynamic_char_array *row = map.rows[r];
        uint64_t start = 0;
        while (start < map.slot_count)
        {
            char status = get_element(row, start);
            uint64_t end = start + 1;
            while (end < map.slot_count && get_element(row, end) == status)
                end++;
            if ((uint64_t)status != map.skip_status && (uint64_t)status < map.status_count)
            {
                fprintf(file, "\\fill[fill=");
                print_color(file, map.status_colors[(uint64_t)status]);
                fprintf(file, "] (%" PRIu64 ",%" PRIu64 ") rectangle (%" PRIu64 ",%" PRIu64 ");\n", start, r, end, r + 1);
            }
            start = end;
        }
        fprintf(file, "\\node[left, font=\\scriptsize] at (0,%.1f) {%s};\n", r + 0.5, map.row_labels[r]);
    }
    fprintf(file, "\\foreach \\s in {0,%" PRIu64 ",...,%" PRIu64 "} {\\draw[gray!30] (\\s,0) -- (\\s,%" PRIu64 "); \\node[below, font=\\scriptsize] at (\\s,%" PRIu64 ") {\\s};}\n",
            step, map.slot_count, map.row_count, map.row_count);
    fprintf(file, "\\draw[gray!60] (0,0) rectangle (%" PRIu64 ",%" PRIu64 ");\n", map.slot_count, map.row_count);
    fprintf(file, "\\node[font=\\small] at (%.1f,%.1f) {FSU};\n", map.slot_count / 2.0, map.row_count + 2.5);
    fprintf(file, "\\node[font=\\small] at (%.1f,%.1f) {", map.slot_count / 2.0, map.row_count + 4.5);
    for (uint64_t i = 0; i < map.status_count; i++)
    {
        if (map.status_names[i] == NULL || i == map.skip_status)
            continue;
        hex_color c = map.status_colors[i];
        fprintf(file, "\\textcolor[RGB]{%d,%d,%d}{\\rule{2.5mm}{2.5mm}} %s\\quad ", c.red, c.green, c.blue, map.status_names[i]);
    }
    fprintf(file, "};\n\\end{tikzpicture}\n");
    end_fit(file);
}

static unsigned char lerp(unsigned char a, unsigned char b, double t)
{
    return (unsigned char)(a + (b - a) * t + 0.5);
}

hex_color color_scale(double t)
{
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    hex_color from = t < 0.5 ? scale_low : scale_mid, to = t < 0.5 ? scale_mid : scale_high;
    double u = t < 0.5 ? t * 2 : (t - 0.5) * 2;
    hex_color ret = {.red = lerp(from.red, to.red, u), .green = lerp(from.green, to.green, u), .blue = lerp(from.blue, to.blue, u)};
    return ret;
}

void print_color_scale_legend(FILE *file, double min, double max, const char *label)
{
    fprintf(file, "\\begin{center}\\begin{tikzpicture}\n");
    fprintf(file, "\\definecolor{scalelow}{RGB}{%d,%d,%d}\\definecolor{scalemid}{RGB}{%d,%d,%d}\\definecolor{scalehigh}{RGB}{%d,%d,%d}\n",
            scale_low.red, scale_low.green, scale_low.blue, scale_mid.red, scale_mid.green, scale_mid.blue, scale_high.red, scale_high.green, scale_high.blue);
    fprintf(file, "\\shade[left color=scalelow, right color=scalemid] (0,0) rectangle (3,0.3);\n"
                  "\\shade[left color=scalemid, right color=scalehigh] (3,0) rectangle (6,0.3);\n"
                  "\\draw[gray] (0,0) rectangle (6,0.3);\n"
                  "\\node[below, font=\\scriptsize] at (0,0) {%.0f};\n"
                  "\\node[below, font=\\scriptsize] at (6,0) {%.0f};\n"
                  "\\node[left, font=\\small] at (0,0.15) {%s};\n",
            min, max, label);
    fprintf(file, "\\end{tikzpicture}\\end{center}\n");
}

void print_latex_escaped(FILE *file, const char *text)
{
    for (; *text; text++)
    {
        switch (*text)
        {
        case '_':
        case '&':
        case '%':
        case '#':
        case '$':
        case '{':
        case '}':
            fprintf(file, "\\%c", *text);
            break;
        case '\\':
            fprintf(file, "\\textbackslash{}");
            break;
        case '~':
            fprintf(file, "\\textasciitilde{}");
            break;
        case '^':
            fprintf(file, "\\textasciicircum{}");
            break;
        default:
            fputc(*text, file);
        }
    }
}

void print_linebreak(FILE *file)
{
    fprintf(file, "\\bigskip\\\\\n");
}

void print_pagebreak(FILE *file)
{
    fprintf(file, "\\clearpage\n");
}

void print_section(FILE *file, const char *section)
{
    fprintf(file, "\\section*{");
    print_latex_escaped(file, section);
    fprintf(file, "}\n");
}

void print_prologue(FILE *file, const char *title)
{
    fprintf(file, "\\documentclass[10pt]{article}\n"
                  "\\usepackage[T1]{fontenc}\n"
                  "\\usepackage{lmodern}\n"
                  "\\usepackage{mathtools}\n"
                  "\\usepackage{booktabs}\n"
                  "\\usepackage{adjustbox}\n"
                  "\\usepackage[left=1.5cm, right=2cm, top=1.5cm, bottom=2cm]{geometry}\n"
                  "\\usepackage{tikz}\n"
                  "\\usepackage{pgfplots}\n"
                  "\\pgfplotsset{compat=1.18}\n"
                  "\\usetikzlibrary{arrows.meta,positioning,calc}\n"
                  "\\setlength\\parindent{0pt}\n"
                  "\\title{");
    print_latex_escaped(file, title);
    fprintf(file, "}\n\\date{}\n\\author{}\n\n\\begin{document}\n\\maketitle\n");
}

void print_epilogue(FILE *file)
{
    fprintf(file, "\n\\end{document}\n");
}
