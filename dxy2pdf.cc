// Copyright © 2017 Jakub Wilk <jwilk@jwilk.net> 
// SPDX-License-Identifier: MIT

#include <climits>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static const char *program_name = "dxy2pdf";

template<typename ...Args>
std::string strfmt(const std::string& format, Args... args)
{
    int size = snprintf(nullptr, 0, format.c_str(), args...);
    if (size < 0 || size == INT_MAX)
        throw std::runtime_error("snprintf() failed");
    std::vector<char> buffer(size + 1);
    sprintf(buffer.data(), format.c_str(), args...);
    return buffer.data();
}

template<typename ...Args>
void warn(const std::string& format, Args... args)
{
    std::string format2 = strfmt("%%s: %s\n", format.c_str());
    std::string message = strfmt(format2, program_name, args...);
    std::cerr << message;
}

class PDF
{
private:
    static constexpr double unit = 25.4 / 72.0;
    std::ostream &file;
    double width, height;
    double x, y;
    size_t offset = 0;
    std::vector<size_t> offsets;
    std::stringstream ops;
    void add(const std::string &s)
    {
        this->offset += s.size();
        this->file << s;
    }
    void add_object(const std::string &s)
    {
        offsets.push_back(this->offset);
        size_t i = this->offsets.size();
        std::string prefix = strfmt("%zu 0 obj\n", i);
        this->add(prefix);
        this->add(s);
        this->add("\nendobj\n");
    }
    template<typename ...Args>
    void add_object(const std::string &format, Args... args)
    {
        std::string s = strfmt(format, args...);
        this->add_object(s);
    }
    template<typename ...Args>
    void add_op(const std::string &format, Args... args)
    {
        ops << strfmt(format, args...);
        ops << "\n";
    }
    const std::string escape(const std::string &s)
    {
        std::stringstream ss;
        ss << '(';
        for (char c : s)
        switch (c) {
            case '\t':
            case '\r':
            case '\n':
                ss << ' ';
                break;
            case '(':
            case ')':
            case '\\':
                ss << strfmt("\\%03o", c);
                break;
            default:
                ss << c;
                break;
        }
        ss << ')';
        return ss.str();
    }
public:
    PDF(std::ostream &file, double width, double height)
    : file(file), width(width), height(height)
    {
        this->add("%PDF-1.1\n");
        this->add_object("<< /Pages 2 0 R >>");
        this->add_object("<< /Kids [3 0 R] /Type /Pages /Count 1 >>");
        this->add_object(
            "<< /Parent 2 0 R " \
            "/Contents 4 0 R " \
            "/MediaBox [0 0 %f %f] " \
            "/Resources  << /Font << /F1 << /BaseFont /Courier /Subtype /Type1 /Type /Font >> >> >> " \
            "/Type /Page " \
            ">>",
            width / this->unit, height / this->unit
        );
        this->add_op(
            "%f 0 0 %f %f %f cm",
            1 / this->unit, 1 / this->unit,
            10 / this->unit, 10 / this->unit  // FIXME: hardcoded offset
        );
        this->add_op("%f w", 0.2); // FIXME: hardcoded line-width
    }
    void move(double x, double y)
    {
        this->x = x;
        this->y = y;
    }
    void draw(double x, double y)
    {
        this->add_op("%f %f m", this->x, this->y);
        this->x = x;
        this->y = y;
        this->add_op("%f %f l s", x, y);
    }
    void rdraw(double dx, double dy)
    {
        this->draw(this->x + dx, this->y + dy);
    }
    void print_text(const std::string &s)
    {
        std::string se = this->escape(s);
        this->add_op(
            "BT /F1 4 Tf %f %f Td %s Tj ET",  // FIXME: hardcoded font-size
            this->x, this->y, se.c_str()
        );
    }
    void finish()
    {
        std::string ops = this->ops.str();
        this->add_object("<< /Length %zu >>\nstream\n%sendstream", ops.size(), ops.c_str());
        this->file << strfmt("xref\n0 %zu\n", 1 + this->offsets.size());
        this->file << strfmt("%010u %05u f\n", 0U, 0xFFFFU);
        for (auto i : this->offsets)
            this->file << strfmt("%010zu %05d n\n", i, 0U);
        this->file << strfmt("trailer\n<< /Root 1 0 R /Size %zu >>\n", 1 + this->offsets.size());
        this->file << strfmt("startxref\n%zu\n", this->offset);
        this->file << "%%EOF\n";
    }
};

static const double a3_width = 297.0;
static const double a3_height = 420.0;

std::string normalize_commas(std::string &s)
{
    return std::regex_replace(s, std::regex(","), " ");
}

void process_file(std::istream &ifile, std::ostream &ofile)
{
    const double dxy_unit = 0.1;
    PDF pdf(ofile, a3_height, a3_width);
    for (std::string line; std::getline(ifile, line);) {
        if (line.size() == 0)
            break;
        char cmd = line[0];
        std::string args = std::string(line, 1);
        switch (cmd)
        {
        case 'D': // draw
        {
            args = normalize_commas(args);
            std::stringstream ss(args);
            double x, y;
            while (ss >> x >> y)
                pdf.draw(x * dxy_unit, y * dxy_unit);
        }
            break;
        case 'I': // relative draw
        {
            args = normalize_commas(args);
            std::stringstream ss(args);
            double x, y;
            while (ss >> x >> y)
                pdf.rdraw(x * dxy_unit, y * dxy_unit);
        }
            break;
        case 'J': // pen change
            // FIXME
            break;
        case 'M': // move
        {
            args = normalize_commas(args);
            std::stringstream ss(args);
            double x, y;
            ss >> x >> y;
            pdf.move(x * dxy_unit, y * dxy_unit);
        }
            // FIXME
            break;
        case 'P': // print text
            pdf.print_text(args);
            break;
        case 'S': // font size
            // FIXME
            break;
        default:
            warn("command not implemented: %c", cmd);
        }
    }
    pdf.finish();
}

void process_file(const char *ipath)
{
    std::string opath = ipath;
    opath += ".pdf";
    std::fstream ifile, ofile;
    ifile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    ifile.open(ipath, std::fstream::in | std::fstream::binary);
    ifile.exceptions(std::ifstream::badbit);
    ofile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    ofile.open(opath, std::fstream::out | std::fstream::binary);
    ofile.exceptions(std::ifstream::badbit);
    process_file(ifile, ofile);
    ofile.close();
    ifile.close();
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        std::cerr << strfmt("Usage: %s <file>...\n", program_name);
        return 1;
    }
    for (int i = 1; i < argc; i++)
        process_file(argv[i]);
    return 0;
}

// vim:ts=4 sts=4 sw=4 et
