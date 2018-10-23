#ifndef _WSM_Q_STR_EXCEPTION_H_
#define _WSM_Q_STR_EXCEPTION_H_

namespace Wsm {

class QStrException {
private:
	QString m_err_msg;

public:
	QStrException(const QString& msg) : m_err_msg(msg) {};
	~QStrException() noexcept {};
	QString str() const noexcept { return this->m_err_msg; };
	operator QString() const { return this->m_err_msg; }
};

}//namespace Wsm

#endif
